// Audio: ES8311 codec (I2C 0x18) on I2S at 16 kHz, 16-bit mono.
// Register sequence follows Espressif's es8311 driver (esp-bsp) for MCLK = 256 x fs.
// Read-aloud uses the ESP8266SAM speech library (a robotic voice that works with no internet).
#pragma once
#include <ESP_I2S.h>
#include <AudioOutput.h>
#include <ESP8266SAM.h>

I2SClass i2s;
bool audioOk = false;
uint8_t volIdx = 2;   // 0..3
static const uint8_t VOL_REG[4] = {0x9F, 0xB4, 0xC8, 0xD8};
static const char *VOL_LBL[4] = {"Quiet", "Medium", "Loud", "Max"};

static bool esW(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

void setVolume(uint8_t i) { volIdx = i % 4; if (audioOk) esW(0x32, VOL_REG[volIdx]); }

bool es8311Init() {
  if (!esW(0x00, 0x1F)) return false;   // reset
  delay(20);
  esW(0x00, 0x00);
  esW(0x00, 0x80);   // power on, slave mode
  esW(0x01, 0x3F);   // all clocks on, MCLK from the MCLK pin
  // clock dividers for MCLK 4.096 MHz, 16 kHz: pre_div 1, mult 1, adc/dac div 1, bclk div 4, osr 0x10
  esW(0x02, 0x00);
  esW(0x03, 0x10);
  esW(0x04, 0x10);
  esW(0x05, 0x00);
  esW(0x06, 0x03);
  esW(0x07, 0x00);
  esW(0x08, 0xFF);
  esW(0x09, 0x0C);   // serial in: I2S, 16-bit
  esW(0x0A, 0x0C);   // serial out: I2S, 16-bit
  esW(0x0D, 0x01);   // power up analog
  esW(0x0E, 0x02);   // PGA and ADC modulator on
  esW(0x12, 0x00);   // DAC on
  esW(0x13, 0x10);   // headphone/line driver on
  esW(0x1C, 0x6A);   // ADC EQ bypass, DC offset cancel
  esW(0x37, 0x08);   // DAC EQ bypass
  esW(0x17, 0xC8);   // ADC digital volume
  esW(0x14, 0x1A);   // analog mic, PGA max
  esW(0x16, MIC_GAIN);
  esW(0x31, 0x00);   // unmute
  esW(0x32, VOL_REG[volIdx]);
  return true;
}

void audioSetup() {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("I2S start failed");
    return;
  }
  delay(10);   // MCLK running before the codec is configured
  audioOk = es8311Init();
  Serial.printf("ES8311 %s\n", audioOk ? "ready" : "not answering");
}

// ---------- playback ----------
static void playSamples(const int16_t *s, size_t n) {
  if (!audioOk) return;
  i2s.write((const uint8_t *)s, n * 2);
}

static void silence(int ms) {
  static int16_t z[256] = {0};
  int n = AUDIO_RATE * ms / 1000;
  while (n > 0) { int k = min(n, 256); playSamples(z, k); n -= k; }
}

static void tone(int hz, int ms, int amp) {
  int16_t buf[256];
  int n = AUDIO_RATE * ms / 1000, i = 0;
  float ph = 0, step = 2 * PI * hz / AUDIO_RATE;
  while (i < n) {
    int k = min(n - i, 256);
    for (int j = 0; j < k; j++, i++) {
      float env = min(1.0f, min(i, n - i) / 160.0f);   // short fade in/out, no clicks
      buf[j] = (int16_t)(sinf(ph) * amp * env);
      ph += step;
    }
    playSamples(buf, k);
  }
}

void chime() {
  if (!audioOk) return;
  tone(1319, 90, 6000);
  tone(1760, 140, 6000);
  silence(40);
}

// ---------- recording ----------
int16_t *recBuf = nullptr;
size_t recLen = 0;
int recPeak = 0;

bool recBegin() {
  if (!audioOk) return false;
  if (!recBuf) recBuf = (int16_t *)ps_malloc(AUDIO_RATE * MAX_REC_SEC * 2);
  if (!recBuf) return false;
  recLen = 0;
  recPeak = 0;
  int16_t junk[256];
  for (int i = 0; i < 8; i++) i2s.readBytes((char *)junk, sizeof junk);   // drop stale samples
  return true;
}

// read the next ~32 ms of audio (blocks until it arrives); returns false when the buffer is full
bool recPoll() {
  size_t cap = AUDIO_RATE * MAX_REC_SEC;
  if (recLen >= cap) return false;
  size_t want = min((size_t)512, cap - recLen);
  size_t got = i2s.readBytes((char *)(recBuf + recLen), want * 2) / 2;
  for (size_t i = 0; i < got; i++) { int a = abs(recBuf[recLen + i]); if (a > recPeak) recPeak = a; }
  recLen += got;
  return recLen < cap;
}

// scale so the loudest sample sits near full scale; speech recognition likes a healthy level
void recNormalize() {
  int peak = 1;
  for (size_t i = 0; i < recLen; i++) peak = max(peak, abs((int)recBuf[i]));
  float g = min(8.0f, 29000.0f / peak);
  if (g <= 1.05f) return;
  for (size_t i = 0; i < recLen; i++) recBuf[i] = (int16_t)(recBuf[i] * g);
}

// ---------- read aloud ----------
// SAM renders at 22.05 kHz; we collect it, then resample to 16 kHz for the codec.
class SamCapture : public AudioOutput {
public:
  int16_t *buf = nullptr;
  size_t n = 0, cap = 0;
  bool begin() override { n = 0; return true; }
  bool ConsumeSample(int16_t s[2]) override {
    if (n < cap) buf[n++] = s[0];
    return true;
  }
  bool stop() override { return true; }
};

static SamCapture samOut;
static ESP8266SAM sam;

// SAM only knows plain letters, digits and basic punctuation
static void speakable(char *dst, const char *src, size_t cap) {
  size_t j = 0;
  for (const uint8_t *p = (const uint8_t *)src; *p && j < cap - 1; p++) {
    uint8_t c = *p;
    if (c >= 0x80) { if ((c & 0xC0) == 0xC0) dst[j++] = ' '; continue; }   // drop non-ASCII, one space per character
    if (c == '\n') c = ' ';
    if (isalnum(c) || strchr(" .,?!'-", c)) dst[j++] = c;
    else if (j && dst[j - 1] != ' ') dst[j++] = ' ';
  }
  dst[j] = 0;
}

bool sayAbort();   // ui.h: true when the user taps to stop

void say(const char *text) {
  if (!audioOk || !text || !*text) return;
  if (!samOut.buf) {
    samOut.cap = 22050 * 20;   // up to 20 seconds of speech
    samOut.buf = (int16_t *)ps_malloc(samOut.cap * 2);
    if (!samOut.buf) return;
  }
  sam.SetVoice(ESP8266SAM::VOICE_SAM);
  char clean[600];
  speakable(clean, text, sizeof clean);
  // SAM takes at most ~250 characters at a time: speak sentence-sized pieces
  const char *p = clean;
  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    int len = strlen(p), cut = len;
    if (len > 200) {
      cut = 200;
      for (int i = 200; i > 80; i--) if (strchr(".?!,", p[i]) || p[i] == ' ') { cut = i + 1; break; }
    }
    char piece[256];
    memcpy(piece, p, cut);
    piece[cut] = 0;
    p += cut;
    samOut.n = 0;
    sam.Say(&samOut, piece);
    // 22050 -> 16000 linear resample, in chunks
    int16_t out[256];
    int k = 0;
    double pos = 0, step = 22050.0 / AUDIO_RATE;
    while ((size_t)pos + 1 < samOut.n) {
      size_t i = (size_t)pos;
      float f = pos - i;
      out[k++] = (int16_t)((samOut.buf[i] * (1 - f) + samOut.buf[i + 1] * f) * 0.8f);
      if (k == 256) {
        playSamples(out, k);
        k = 0;
        if (sayAbort()) { silence(30); return; }
      }
      pos += step;
    }
    if (k) playSamples(out, k);
  }
  silence(60);
}

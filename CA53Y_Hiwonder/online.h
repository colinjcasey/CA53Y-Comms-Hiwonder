// Everything that needs the user's own Wi-Fi: clock (NTP), network cameras (MJPEG / JPEG snapshot),
// speech-to-text (any OpenAI-compatible /audio/transcriptions server) and the web settings page.
// Nothing here is hardcoded: the user enters their network, cameras and server on the device or the web page.
//
// The CA53Y mesh lives on Wi-Fi channel 1. A radio can only be on one channel, so:
//   - if the user's router is on channel 1, the board stays on Wi-Fi and the mesh keeps working;
//   - otherwise it joins Wi-Fi only while something needs it (camera on screen, voice-to-text, clock sync,
//     web settings) and goes back to channel 1 afterwards. The status bar says when the mesh is paused.
#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <time.h>
#include <mbedtls/base64.h>

// ---------- settings ----------
struct Cam { char name[24]; char url[160]; };
Cam cams[MAX_CAMS];
int nCams = 0;
char wSsid[33], wPass[65], webCode[8];
bool wifiOn = false, h24 = false;
uint8_t tzIdx = 0;
int8_t homeCam = -1;               // -1 = logo / clock; otherwise the camera shown on the home screen
char sttUrl[160], sttKey[120], sttModel[40], sttLang[8];

struct Tz { const char *name, *posix; };
static const Tz TZS[] = {
  {"UTC", "UTC0"},
  {"US Eastern", "EST5EDT,M3.2.0,M11.1.0"},
  {"US Central", "CST6CDT,M3.2.0,M11.1.0"},
  {"US Mountain", "MST7MDT,M3.2.0,M11.1.0"},
  {"US Arizona", "MST7"},
  {"US Pacific", "PST8PDT,M3.2.0,M11.1.0"},
  {"US Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
  {"US Hawaii", "HST10"},
  {"UK / Ireland", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Central Europe", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Eastern Europe", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"South Africa", "SAST-2"},
  {"India", "IST-5:30"},
  {"China / Singapore", "CST-8"},
  {"Japan / Korea", "JST-9"},
  {"Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};
#define N_TZ ((int)(sizeof TZS / sizeof TZS[0]))

void saveCams() {
  for (int i = 0; i < MAX_CAMS; i++) {
    char k[6];
    snprintf(k, sizeof k, "cn%d", i);
    if (i < nCams) prefs.putString(k, cams[i].name); else prefs.remove(k);
    snprintf(k, sizeof k, "cu%d", i);
    if (i < nCams) prefs.putString(k, cams[i].url); else prefs.remove(k);
  }
  if (homeCam >= nCams) { homeCam = -1; prefs.putChar("home", -1); }
}

void applyTz() {
  setenv("TZ", TZS[tzIdx].posix, 1);
  tzset();
}

void loadOnline() {
  strlcpy(wSsid, prefs.getString("wssid", "").c_str(), sizeof wSsid);
  strlcpy(wPass, prefs.getString("wpass", "").c_str(), sizeof wPass);
  wifiOn = prefs.getBool("won", false) && wSsid[0];
  h24 = prefs.getBool("h24", false);
  tzIdx = prefs.getUChar("tz", 0) % N_TZ;
  homeCam = prefs.getChar("home", -1);
  strlcpy(sttUrl, prefs.getString("stt", "").c_str(), sizeof sttUrl);
  strlcpy(sttKey, prefs.getString("sttk", "").c_str(), sizeof sttKey);
  strlcpy(sttModel, prefs.getString("sttm", "whisper-1").c_str(), sizeof sttModel);
  strlcpy(sttLang, prefs.getString("sttl", "en").c_str(), sizeof sttLang);
  String wc = prefs.getString("webc", "");
  if (wc.length() != 6) { wc = String(100000 + esp_random() % 900000); prefs.putString("webc", wc); }
  strlcpy(webCode, wc.c_str(), sizeof webCode);
  nCams = 0;
  for (int i = 0; i < MAX_CAMS; i++) {
    char k[6];
    snprintf(k, sizeof k, "cu%d", i);
    String u = prefs.getString(k, "");
    if (!u.length()) continue;
    strlcpy(cams[nCams].url, u.c_str(), sizeof cams[nCams].url);
    snprintf(k, sizeof k, "cn%d", i);
    strlcpy(cams[nCams].name, prefs.getString(k, "").c_str(), sizeof cams[nCams].name);
    if (!cams[nCams].name[0]) snprintf(cams[nCams].name, sizeof cams[nCams].name, "Camera %d", nCams + 1);
    nCams++;
  }
  if (homeCam >= nCams) homeCam = -1;
  applyTz();
}

// ---------- URLs ----------
struct Url { bool tls; String host, path, user, pass; uint16_t port; };

bool parseUrl(const char *s, Url &u) {
  String a(s);
  a.trim();
  if (a.startsWith("https://")) { u.tls = true; a = a.substring(8); u.port = 443; }
  else if (a.startsWith("http://")) { u.tls = false; a = a.substring(7); u.port = 80; }
  else return false;
  int slash = a.indexOf('/');
  String hostPart = slash < 0 ? a : a.substring(0, slash);
  u.path = slash < 0 ? "/" : a.substring(slash);
  int at = hostPart.lastIndexOf('@');
  u.user = ""; u.pass = "";
  if (at >= 0) {
    String cred = hostPart.substring(0, at);
    hostPart = hostPart.substring(at + 1);
    int c = cred.indexOf(':');
    u.user = c < 0 ? cred : cred.substring(0, c);
    u.pass = c < 0 ? "" : cred.substring(c + 1);
  }
  int colon = hostPart.lastIndexOf(':');
  if (colon >= 0) { u.port = hostPart.substring(colon + 1).toInt(); hostPart = hostPart.substring(0, colon); }
  u.host = hostPart;
  return u.host.length() > 0 && u.port > 0;
}

String basicAuth(const String &user, const String &pass) {
  String up = user + ":" + pass;
  unsigned char out[200];
  size_t olen = 0;
  if (mbedtls_base64_encode(out, sizeof out - 1, &olen, (const unsigned char *)up.c_str(), up.length()) != 0) return "";
  out[olen] = 0;
  return String("Basic ") + (const char *)out;
}

// ---------- Wi-Fi ----------
enum : uint8_t { W_OFF, W_JOINING, W_UP };
uint8_t wState = W_OFF;
uint8_t routerChan = 0;              // learned on first connect; 0 = not known yet
uint32_t wJoinAt = 0, wRetryAt = 0, lastSync = 0, wUpAt = 0;
bool camWant = false, sttWant = false, webWant = false;
char wErr[48] = "";
WebServer web(80);
bool webStarted = false;
void webSetup();

bool timeValid() { return time(nullptr) > 1700000000; }
bool meshPaused() { return wState == W_UP && WiFi.channel() != ESPNOW_CHANNEL; }
bool wifiUp() { return wState == W_UP; }

static void backToMesh() {
  WiFi.disconnect(false, false);
  delay(20);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

void wifiForget() {
  wSsid[0] = wPass[0] = 0;
  wifiOn = false;
  routerChan = 0;
  prefs.remove("wssid"); prefs.remove("wpass"); prefs.putBool("won", false);
  if (wState != W_OFF) backToMesh();
  wState = W_OFF;
  netChanged = true;
}

void wifiSet(const char *ssid, const char *pass) {
  strlcpy(wSsid, ssid, sizeof wSsid);
  strlcpy(wPass, pass, sizeof wPass);
  wifiOn = true;
  routerChan = 0;
  wErr[0] = 0;
  prefs.putString("wssid", wSsid); prefs.putString("wpass", wPass); prefs.putBool("won", true);
  if (wState != W_OFF) backToMesh();
  wState = W_OFF;
  wRetryAt = 0;
  netChanged = true;
}

void wifiLoop() {
  uint32_t now = millis();
  bool syncDue = !timeValid() || now - lastSync > 6UL * 3600 * 1000;
  bool stay = routerChan == ESPNOW_CHANNEL;
  bool want = wifiOn && (stay || routerChan == 0 || camWant || sttWant || webWant || syncDue);

  switch (wState) {
    case W_OFF:
      if (want && (int32_t)(now - wRetryAt) >= 0) {
        WiFi.setAutoReconnect(false);
        WiFi.begin(wSsid, wPass);
        wState = W_JOINING;
        wJoinAt = now;
        netChanged = true;
      }
      break;
    case W_JOINING:
      if (WiFi.status() == WL_CONNECTED) {
        wState = W_UP;
        wUpAt = now;
        routerChan = WiFi.channel();
        wErr[0] = 0;
        configTzTime(TZS[tzIdx].posix, "pool.ntp.org", "time.nist.gov");
        if (!webStarted) { webSetup(); webStarted = true; }
        Serial.printf("Wi-Fi up: %s on channel %d (%s)\n", WiFi.localIP().toString().c_str(), routerChan,
                      routerChan == ESPNOW_CHANNEL ? "mesh keeps running" : "mesh pauses while joined");
        netChanged = true;
      } else if (!wifiOn || now - wJoinAt > 15000) {
        strlcpy(wErr, WiFi.status() == WL_NO_SSID_AVAIL ? "Network not found" : "Couldn't join (check the password)", sizeof wErr);
        backToMesh();
        wState = W_OFF;
        wRetryAt = now + 60000;
        netChanged = true;
      }
      break;
    case W_UP:
      if (WiFi.status() != WL_CONNECTED) {
        backToMesh();
        wState = W_OFF;
        wRetryAt = now + 5000;
        netChanged = true;
        break;
      }
      if (syncDue && timeValid() && now - wUpAt > 6000) lastSync = now;   // give SNTP a moment to actually re-sync
      if (!want) {   // nothing needs Wi-Fi and the router is on another channel: give the radio back to the mesh
        backToMesh();
        wState = W_OFF;
        wRetryAt = now + 2000;
        netChanged = true;
        break;
      }
      web.handleClient();
      break;
  }
}

// ---------- network camera (MJPEG stream or JPEG snapshot URL) ----------
// We don't parse the multipart headers: we pull whole JPEGs out of the byte stream by their start/end markers.
// That works for ESP32-CAM style /stream URLs and for /capture or snapshot.jpg URLs (re-fetched).
int camIdx = -1;
WiFiClient camCli;
uint8_t *camBuf = nullptr;
size_t camLen = 0, camCap = 300 * 1024;
bool camIn = false, camReady = false, camHeader = false;
uint8_t camPrev = 0;
uint32_t camRetryAt = 0, camLastData = 0, camFrames = 0;
char camErr[64] = "";

void camStop() {
  if (camCli.connected()) camCli.stop();
  camIdx = -1;
  camReady = false;
}

void camStart(int i) {
  if (camCli.connected()) camCli.stop();
  camIdx = i;
  camLen = 0; camIn = false; camReady = false; camFrames = 0;
  camErr[0] = 0;
  camRetryAt = 0;
}

static void camConnect() {
  Url u;
  if (!parseUrl(cams[camIdx].url, u)) { strlcpy(camErr, "That camera address isn't a valid http:// URL", sizeof camErr); return; }
  if (u.tls) { strlcpy(camErr, "https cameras aren't supported; use the http:// address", sizeof camErr); return; }
  camCli.setTimeout(3000);   // Stream timeout, milliseconds
  if (!camCli.connect(u.host.c_str(), u.port, 3000)) {
    snprintf(camErr, sizeof camErr, "Can't reach %s", u.host.c_str());
    camRetryAt = millis() + 3000;
    return;
  }
  String req = "GET " + u.path + " HTTP/1.1\r\nHost: " + u.host + "\r\nUser-Agent: CA53Y\r\nConnection: close\r\n";
  if (u.user.length()) req += "Authorization: " + basicAuth(u.user, u.pass) + "\r\n";
  req += "\r\n";
  camCli.print(req);
  camHeader = true;
  camLastData = millis();
}

void camLoop() {
  if (camIdx < 0 || camIdx >= nCams || !wifiUp() || (camErr[0] && camRetryAt == 0)) return;
  if (camReady) return;   // last frame not drawn yet: leave the rest in the socket
  if (!camBuf) { camBuf = (uint8_t *)ps_malloc(camCap); if (!camBuf) return; }
  uint32_t now = millis();
  if (!camCli.connected() && !camCli.available()) {
    if ((int32_t)(now - camRetryAt) < 0) return;
    camErr[0] = 0;
    camConnect();
    if (!camCli.connected()) return;
  }
  if (camHeader) {   // status line
    if (!camCli.available()) {
      if (now - camLastData > 5000) { strlcpy(camErr, "Camera didn't answer", sizeof camErr); camCli.stop(); camRetryAt = now + 3000; }
      return;
    }
    String st = camCli.readStringUntil('\n');
    camHeader = false;
    int code = st.length() > 12 ? st.substring(9, 12).toInt() : 0;
    if (code == 401) { strlcpy(camErr, "Needs a password: http://user:pass@ip/...", sizeof camErr); camCli.stop(); camRetryAt = 0; return; }
    if (code != 200) { snprintf(camErr, sizeof camErr, "Camera replied %d", code); camCli.stop(); camRetryAt = now + 5000; return; }
  }
  uint8_t tmp[2048];
  int budget = 64 * 1024;   // don't hog the loop
  while (budget > 0 && camCli.available()) {
    int n = camCli.read(tmp, min((int)sizeof tmp, budget));
    if (n <= 0) break;
    budget -= n;
    camLastData = now;
    for (int i = 0; i < n; i++) {
      uint8_t b = tmp[i];
      if (!camIn) {
        if (camPrev == 0xFF && b == 0xD8) { camIn = true; camBuf[0] = 0xFF; camBuf[1] = 0xD8; camLen = 2; }
      } else {
        if (camLen < camCap) camBuf[camLen++] = b; else camIn = false;   // too big: drop it
        if (camPrev == 0xFF && b == 0xD9) { camIn = false; camReady = true; camFrames++; camPrev = 0; return; }
      }
      camPrev = b;
    }
  }
  if (camCli.connected() && now - camLastData > 8000) { strlcpy(camErr, "Stream stalled, reconnecting", sizeof camErr); camCli.stop(); camRetryAt = now + 1000; }
  if (!camCli.connected() && !camCli.available()) camRetryAt = now + 300;   // snapshot URLs: fetch again shortly
}

bool jpegSize(const uint8_t *d, size_t n, int &w, int &h) {
  for (size_t i = 2; i + 8 < n; ) {
    if (d[i] != 0xFF) { i++; continue; }
    uint8_t m = d[i + 1];
    if (m == 0xC0 || m == 0xC1 || m == 0xC2) { h = (d[i + 5] << 8) | d[i + 6]; w = (d[i + 7] << 8) | d[i + 8]; return w > 0 && h > 0; }
    if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
    i += 2 + ((d[i + 2] << 8) | d[i + 3]);
  }
  return false;
}

// ---------- speech-to-text ----------
// POSTs the recording as a WAV file to an OpenAI-compatible transcription endpoint, for example
// https://api.openai.com/v1/audio/transcriptions or a local Whisper server's /v1/audio/transcriptions.
static String jsonText(const String &body) {
  int k = body.indexOf("\"text\"");
  if (k < 0) return "";
  k = body.indexOf('"', body.indexOf(':', k) + 1);
  if (k < 0) return "";
  String out;
  for (int i = k + 1; i < (int)body.length(); i++) {
    char c = body[i];
    if (c == '"') break;
    if (c != '\\') { out += c; continue; }
    char e = body[++i];
    if (e == 'n') out += ' ';
    else if (e == 'u') {
      long cp = strtol(body.substring(i + 1, i + 5).c_str(), nullptr, 16);
      i += 4;
      if (cp < 0x80) out += (char)cp;
      else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
      else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    } else out += e;
  }
  out.trim();
  return out;
}

static void wavHeader(uint8_t *h, uint32_t samples) {
  uint32_t data = samples * 2, rate = AUDIO_RATE, br = AUDIO_RATE * 2;
  memcpy(h, "RIFF", 4); uint32_t riff = 36 + data; memcpy(h + 4, &riff, 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  uint32_t fmtLen = 16; uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  memcpy(h + 16, &fmtLen, 4); memcpy(h + 20, &pcm, 2); memcpy(h + 22, &ch, 2);
  memcpy(h + 24, &rate, 4); memcpy(h + 28, &br, 4); memcpy(h + 32, &align, 2); memcpy(h + 34, &bits, 2);
  memcpy(h + 36, "data", 4); memcpy(h + 40, &data, 4);
}

// returns the text, or "" with err set
String transcribe(const int16_t *pcm, size_t samples, String &err) {
  Url u;
  if (!sttUrl[0] || !parseUrl(sttUrl, u)) { err = "Set a speech-to-text server in web settings"; return ""; }
  WiFiClientSecure tls;
  WiFiClient plain;
  Client *c = &plain;
  if (u.tls) { tls.setInsecure(); c = &tls; }   // no certificate check: see README
  if (!c->connect(u.host.c_str(), u.port)) { err = "Can't reach the speech server"; return ""; }
  const char *B = "----CA53Yboundary7MA4YWxk";
  String pre;
  pre += String("--") + B + "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n";
  if (sttModel[0]) pre += String("--") + B + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n" + sttModel + "\r\n";
  if (sttLang[0]) pre += String("--") + B + "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n" + sttLang + "\r\n";
  pre += String("--") + B + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String post = String("\r\n--") + B + "--\r\n";
  size_t total = pre.length() + 44 + samples * 2 + post.length();
  String hdr = "POST " + u.path + " HTTP/1.0\r\nHost: " + u.host + "\r\nUser-Agent: CA53Y\r\n";
  if (sttKey[0]) hdr += String("Authorization: Bearer ") + sttKey + "\r\n";
  else if (u.user.length()) hdr += "Authorization: " + basicAuth(u.user, u.pass) + "\r\n";
  hdr += String("Content-Type: multipart/form-data; boundary=") + B + "\r\nContent-Length: " + total + "\r\n\r\n";
  c->print(hdr);
  c->print(pre);
  uint8_t wh[44];
  wavHeader(wh, samples);
  c->write(wh, 44);
  const uint8_t *p = (const uint8_t *)pcm;
  size_t left = samples * 2;
  while (left) {
    size_t k = c->write(p, min(left, (size_t)4096));
    if (!k) { err = "Upload failed"; c->stop(); return ""; }
    p += k; left -= k;
  }
  c->print(post);
  uint32_t t0 = millis();
  String resp;
  while (millis() - t0 < 30000) {
    while (c->available()) { resp += (char)c->read(); t0 = millis(); }
    if (!c->connected() && !c->available()) break;
    delay(5);
  }
  c->stop();
  int sp = resp.indexOf(' ');
  int code = sp > 0 ? resp.substring(sp + 1, sp + 4).toInt() : 0;
  int body = resp.indexOf("\r\n\r\n");
  String b = body >= 0 ? resp.substring(body + 4) : "";
  if (code != 200) {
    err = code == 401 ? "Speech server rejected the API key" : code ? "Speech server error " + String(code) : "No reply from the speech server";
    Serial.printf("STT %d: %s\n", code, b.substring(0, 300).c_str());
    return "";
  }
  String t = jsonText(b);
  if (!t.length()) err = "Didn't catch any words";
  return t;
}

// ---------- web settings page ----------
static String esc(const char *s) {
  String o;
  for (; *s; s++) {
    if (*s == '<') o += "&lt;"; else if (*s == '>') o += "&gt;"; else if (*s == '&') o += "&amp;";
    else if (*s == '"') o += "&quot;"; else o += *s;
  }
  return o;
}

static bool webAuth() {
  if (web.authenticate("ca53y", webCode)) return true;
  web.requestAuthentication(BASIC_AUTH, "CA53Y", "User name: ca53y. Password: the code under Settings > Web settings on the device.");
  return false;
}

static void webPage() {
  if (!webAuth()) return;
  String h;
  h.reserve(9000);
  h += F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>CA53Y settings</title><style>"
         "body{margin:0;background:#0F2A30;color:#F2EBD9;font:16px/1.5 system-ui,sans-serif}"
         "main{max-width:640px;margin:0 auto;padding:20px}h1{font:400 32px Georgia,serif;color:#EAD9A8;margin:8px 0 2px}"
         "h2{font:400 22px Georgia,serif;color:#EAD9A8;margin:28px 0 6px;border-bottom:1px solid #2D545A;padding-bottom:4px}"
         "p{color:#A3B8B2;font-size:14px;margin:4px 0 10px}label{display:block;font-size:14px;color:#A3B8B2;margin-top:10px}"
         "input,select{width:100%;box-sizing:border-box;padding:9px;background:#17393F;color:#F2EBD9;border:1px solid #2D545A;border-radius:2px;font:inherit}"
         ".row{display:grid;grid-template-columns:1fr 2fr;gap:8px;margin-top:8px}"
         "button{margin-top:24px;background:#CFA84E;color:#1A2A2C;border:0;padding:12px 22px;font:600 16px system-ui;border-radius:2px}"
         ".ok{border-left:4px solid #86C2AE;padding:6px 12px;background:#17393F}</style></head><body><main>");
  h += F("<h1>CA53Y</h1><p>Settings for ");
  h += esc(myNick[0] ? myNick : myId);
  h += F(" (");
  h += myId;
  h += F(")</p>");
  if (web.hasArg("saved")) h += F("<p class=ok>Saved.</p>");
  h += F("<form method=post action=/save><h2>Cameras</h2>"
         "<p>MJPEG stream or JPEG snapshot addresses on your network, for example <code>http://192.168.1.50:81/stream</code>. "
         "For a camera with a login, use <code>http://user:password@address/...</code>.</p>");
  for (int i = 0; i < MAX_CAMS; i++) {
    h += F("<div class=row><input name=cn");
    h += i;
    h += F(" placeholder='Name' value=\"");
    if (i < nCams) h += esc(cams[i].name);
    h += F("\"><input name=cu");
    h += i;
    h += F(" placeholder='http://...' value=\"");
    if (i < nCams) h += esc(cams[i].url);
    h += F("\"></div>");
  }
  h += F("<label>Home screen</label><select name=home><option value=-1>CA53Y logo and clock</option>");
  for (int i = 0; i < nCams; i++) {
    h += F("<option value=");
    h += i;
    if (homeCam == i) h += F(" selected");
    h += '>';
    h += esc(cams[i].name);
    h += F("</option>");
  }
  h += F("</select><h2>Clock</h2><label>Time zone</label><select name=tz>");
  for (int i = 0; i < N_TZ; i++) {
    h += F("<option value=");
    h += i;
    if (tzIdx == i) h += F(" selected");
    h += '>';
    h += TZS[i].name;
    h += F("</option>");
  }
  h += F("</select><label><input type=checkbox name=h24 value=1 style='width:auto'");
  if (h24) h += F(" checked");
  h += F("> 24-hour clock</label>");
  h += F("<h2>Voice to text</h2><p>Hold Talk to speak a message. The recording is sent to this server and comes back as text "
         "before you send it. Any OpenAI-compatible <code>/v1/audio/transcriptions</code> endpoint works: OpenAI, or a Whisper "
         "server on your own network. Leave the address empty to turn this off.</p>"
         "<label>Server address</label><input name=stt placeholder='https://api.openai.com/v1/audio/transcriptions' value=\"");
  h += esc(sttUrl);
  h += F("\"><label>API key</label><input name=sttk type=password placeholder='");
  h += sttKey[0] ? F("Saved. Leave empty to keep it.") : F("Only if your server needs one");
  h += F("'><label><input type=checkbox name=sttclear value=1 style='width:auto'> Remove the saved key</label>"
         "<div class=row><div><label>Model</label><input name=sttm value=\"");
  h += esc(sttModel);
  h += F("\"></div><div><label>Language code</label><input name=sttl value=\"");
  h += esc(sttLang);
  h += F("\"></div></div><button type=submit>Save</button></form></main></body></html>");
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "text/html", h);
}

static void webSave() {
  if (!webAuth()) return;
  Cam nc[MAX_CAMS];
  int n = 0;
  for (int i = 0; i < MAX_CAMS; i++) {
    String u = web.arg("cu" + String(i)); u.trim();
    if (!u.length()) continue;
    String nm = web.arg("cn" + String(i)); nm.trim();
    strlcpy(nc[n].url, u.c_str(), sizeof nc[n].url);
    if (nm.length()) strlcpy(nc[n].name, nm.c_str(), sizeof nc[n].name);
    else snprintf(nc[n].name, sizeof nc[n].name, "Camera %d", n + 1);
    n++;
  }
  memcpy(cams, nc, sizeof nc);
  nCams = n;
  homeCam = web.arg("home").toInt();
  if (homeCam < -1 || homeCam >= nCams) homeCam = -1;
  prefs.putChar("home", homeCam);
  saveCams();
  tzIdx = constrain(web.arg("tz").toInt(), 0, N_TZ - 1);
  prefs.putUChar("tz", tzIdx);
  applyTz();
  h24 = web.arg("h24") == "1";
  prefs.putBool("h24", h24);
  strlcpy(sttUrl, web.arg("stt").c_str(), sizeof sttUrl);
  prefs.putString("stt", sttUrl);
  if (web.arg("sttclear") == "1") sttKey[0] = 0;
  else if (web.arg("sttk").length()) strlcpy(sttKey, web.arg("sttk").c_str(), sizeof sttKey);
  prefs.putString("sttk", sttKey);
  strlcpy(sttModel, web.arg("sttm").c_str(), sizeof sttModel);
  prefs.putString("sttm", sttModel);
  strlcpy(sttLang, web.arg("sttl").c_str(), sizeof sttLang);
  prefs.putString("sttl", sttLang);
  if (camIdx >= nCams) camStop();
  netChanged = true;
  web.sendHeader("Location", "/?saved=1");
  web.send(303);
}

void webSetup() {
  web.on("/", HTTP_GET, webPage);
  web.on("/save", HTTP_POST, webSave);
  web.onNotFound([]() { web.send(404, "text/plain", "Not found"); });
  web.begin();
}

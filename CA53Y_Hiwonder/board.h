// Board layer for the Hiwonder ESP32-S3 Development Board.
//   LCD: ST7789 240x320 on SPI (SCLK 21, MOSI 47, CS 2, DC 3, no reset pin), shown landscape.
//   XL9555 I/O expander on its own I2C bus (SDA 38, SCL 48): pin 14 backlight, pin 15 audio power.
//   Capacitive touch at 0x38 (FT6336-style), normally on the main I2C bus (SDA 4, SCL 5).
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Wire.h>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX() {
    {
      auto c = _bus.config();
      c.spi_host = SPI2_HOST;
      c.spi_mode = 0;
      c.freq_write = LCD_SPI_HZ;
      c.freq_read = 16000000;
      c.spi_3wire = false;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = LCD_SCLK;
      c.pin_mosi = LCD_MOSI;
      c.pin_miso = -1;
      c.pin_dc = LCD_DC;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    {
      auto c = _panel.config();
      c.pin_cs = LCD_CS;
      c.pin_rst = -1;
      c.pin_busy = -1;
      c.panel_width = 240;
      c.panel_height = 320;
      c.offset_x = 0;
      c.offset_y = 0;
      c.offset_rotation = 0;
      c.readable = false;
      c.invert = true;
      c.rgb_order = false;
      c.dlen_16bit = false;
      c.bus_shared = false;
      _panel.config(c);
    }
    setPanel(&_panel);
  }
};

// ---------- XL9555 I/O expander ----------
static bool xlOk = false;
static uint8_t xlOut[2] = {0xFF, 0xFF}, xlCfg[2] = {0xFF, 0xFF};   // power-on defaults: all inputs, latches high

static bool xlWrite(uint8_t reg, uint8_t v) {
  Wire1.beginTransmission(XL9555_ADDR);
  Wire1.write(reg);
  Wire1.write(v);
  return Wire1.endTransmission() == 0;
}

static bool xlRead(uint8_t reg, uint8_t *v) {
  Wire1.beginTransmission(XL9555_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((int)XL9555_ADDR, 1) != 1) return false;
  *v = Wire1.read();
  return true;
}

void xlPin(int pin, bool high) {
  if (!xlOk) return;
  int port = pin >> 3, bit = pin & 7;
  if (high) xlOut[port] |= (1 << bit); else xlOut[port] &= ~(1 << bit);
  xlCfg[port] &= ~(1 << bit);   // make it an output
  xlWrite(0x02 + port, xlOut[port]);
  xlWrite(0x06 + port, xlCfg[port]);
}

uint16_t xlInputs() {
  uint8_t a = 0xFF, b = 0xFF;
  if (!xlOk) return 0xFFFF;
  xlRead(0x00, &a);
  xlRead(0x01, &b);
  return a | (b << 8);
}

void backlight(bool on) { xlPin(XL_BACKLIGHT, on); }

// ---------- touch ----------
static TwoWire *touchBus = nullptr;
bool flip180 = false;   // Settings > Rotate screen

static bool probe(TwoWire &w, uint8_t addr) {
  w.beginTransmission(addr);
  return w.endTransmission() == 0;
}

// returns true while a finger is down; x,y in screen coordinates (landscape 320x240)
bool touchRead(int &x, int &y) {
  if (!touchBus) return false;
  touchBus->beginTransmission(TOUCH_ADDR);
  touchBus->write(0x02);
  if (touchBus->endTransmission(false) != 0) return false;
  if (touchBus->requestFrom((int)TOUCH_ADDR, 5) != 5) return false;
  uint8_t b[5];
  for (int i = 0; i < 5; i++) b[i] = touchBus->read();
  int n = b[0] & 0x0F;
  if (n == 0 || n > 2) return false;
  int rx = ((b[1] & 0x0F) << 8) | b[2];
  int ry = ((b[3] & 0x0F) << 8) | b[4];
  x = ry;                    // the panel's native frame is portrait; same mapping as Hiwonder's firmware
  y = SCREEN_H - 1 - rx;
  if (flip180) { x = SCREEN_W - 1 - x; y = SCREEN_H - 1 - y; }
  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);
  return true;
}

void boardSetup() {
  Wire.begin(I2C_MAIN_SDA, I2C_MAIN_SCL, 400000);
  Wire1.begin(I2C_EXP_SDA, I2C_EXP_SCL, 400000);
  xlOk = probe(Wire1, XL9555_ADDR);
  if (xlOk) {
    xlPin(XL_AUDIO_EN, true);
    xlPin(XL_BACKLIGHT, true);
  }
  delay(20);
  if (probe(Wire, TOUCH_ADDR)) touchBus = &Wire;
  else if (probe(Wire1, TOUCH_ADDR)) touchBus = &Wire1;
  Serial.printf("XL9555 %s, touch %s, codec %s\n", xlOk ? "ok" : "MISSING",
                touchBus == &Wire ? "on main bus" : touchBus ? "on expander bus" : "MISSING",
                probe(Wire, ES8311_ADDR) ? "ok" : "MISSING");
}

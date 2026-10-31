// CA53Y Comms for the Hiwonder ESP32-S3 Development Board (AI Vision & Smart Chat).
// Pin map from a working XiaoZhi board definition for this board (HiwonderExploit_S3).
#pragma once

#define FW_VERSION "1.1.0"

// ---------- screen: ST7789 240x320 on SPI, shown landscape 320x240 ----------
#define LCD_SCLK        21
#define LCD_MOSI        47
#define LCD_CS          2
#define LCD_DC          3
#define LCD_SPI_HZ      40000000
#define SCREEN_W        320
#define SCREEN_H        240

// ---------- I2C ----------
#define I2C_MAIN_SDA    4      // ES8311 codec, touch controller, camera
#define I2C_MAIN_SCL    5
#define I2C_EXP_SDA     38     // XL9555 I/O expander (backlight, audio power)
#define I2C_EXP_SCL     48
#define XL9555_ADDR     0x20
#define XL_BACKLIGHT    14     // expander pin numbers 0..15
#define XL_AUDIO_EN     15
#define TOUCH_ADDR      0x38   // FT6336-style capacitive touch
#define ES8311_ADDR     0x18

// ---------- audio: ES8311 codec on I2S ----------
#define I2S_MCLK        45
#define I2S_BCLK        39
#define I2S_WS          41
#define I2S_DOUT        42     // ESP32 -> codec (speaker)
#define I2S_DIN         40     // codec -> ESP32 (microphone)
#define AUDIO_RATE      16000
#define MIC_GAIN        4      // ES8311 ADC gain step, 0..7 = 0..42 dB
#define MAX_REC_SEC     8

#define PIN_BOOT        0      // short press: home; hold 5 s: erase everything

// ---------- CA53Y mesh (must match every CA53Y device) ----------
#ifndef LONG_RANGE
#define LONG_RANGE      0   // 0 = Standard network, 1 = Long Range network. Standard and Long Range devices can't hear each other.
#endif
#define ESPNOW_CHANNEL  1
#define MAX_HOPS        8
#define SEND_TRIES      6
#define RETRY_MS        1200
#define CHAN_SENDS      2
#define BEACON_MS       3000
#define NEARBY_TTL_MS   15000
#define KDF_ITERS       2000

#define MAX_CONTACTS    16
#define MAX_CHANNELS    8
#define MAX_NEARBY      20
#define MAX_MSGS        60
#define MAX_OUT         8
#define MAX_TEXT        160
#define MAX_CHAN_TEXT   140

// ---------- network extras ----------
#define MAX_CAMS        6
#define DEVICE_KIND     "Hiwonder S3"

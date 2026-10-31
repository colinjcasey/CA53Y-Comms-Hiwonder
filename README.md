# CA53Y Comms for the Hiwonder ESP32-S3 Development Board

Off-grid CA53Y messaging on the **Hiwonder ESP32-S3 Development Board (AI Vision & Smart Chat)**: 2.0" capacitive touchscreen, speaker and microphone. It joins the CA53Y mesh as a messenger and a repeater as soon as it's flashed, and talks to every other CA53Y device.

## What it does

- **Messages** to contacts, open or encrypted with a shared PIN (AES-128-GCM), with delivery receipts and retries. **Channels** for group chat, open or with a passphrase. A **Nearby** list of devices in direct range.
- **Repeats** every CA53Y message it hears, up to 8 hops, including encrypted ones it can't read.
- **Home screen:** the CA53Y IND logo; a clock once it's been on Wi-Fi; or one of your cameras.
- **New messages fill the screen** with Play (read aloud) and Reply. Optionally reads every new message aloud.
- **Hold to talk:** speak a message, check the text, then send it. Needs Wi-Fi and a speech-to-text server you choose.
- **Cameras:** view MJPEG streams or JPEG snapshot URLs from your network (ESP32-CAM style `http://ip:81/stream` and similar). Pick one as the home screen.
- **Web settings page** on your network for cameras, the speech server, time zone and home screen.

Nothing personal is built into the firmware. Wi-Fi, cameras and the speech server are entered by whoever owns the board and saved on it.

## Standard or Long Range

The firmware comes in two networks, set with `LONG_RANGE` in `config.h` (or `-DLONG_RANGE=1`):

| Network        | `LONG_RANGE` | Radio                                                                  |
|----------------|--------------|------------------------------------------------------------------------|
| **Standard**   | `0`          | Normal ESP-NOW                                                         |
| **Long Range** | `1`          | Mesh messages go out in Espressif's Long Range (LR) mode at 250 kbps    |

**Standard and Long Range devices can't hear each other.** Use the same setting as the messengers and relays
in your group ([CA53Y Comms](https://github.com/colinjcasey/CA53Y-Comms) has both). On this board the Long
Range build keeps normal Wi-Fi enabled alongside LR, so online mode, Wi-Fi scans and cameras still work.
Settings > About shows which network it's on. Long Range installs from the web flasher keep your settings
unless you tick Erase.

## Flash it

### Easiest: from the browser

Open the project's web flasher (GitHub Pages, see below) in **Chrome or Edge** on a computer, plug the board in with a USB-C *data* cable, and click **Install**.

### Arduino IDE

1. **Boards Manager:** install **esp32 by Espressif Systems** 3.x (built and tested on 3.3.12).
2. **Library Manager:** install **LovyanGFX** (1.2.33), **ESP8266Audio** (2.4.1) and **ESP8266SAM** (1.1.0).
3. Open `CA53Y_Hiwonder/CA53Y_Hiwonder.ino`. Keep the folder name the same as the `.ino`.
4. Tools: Board **ESP32S3 Dev Module**, PSRAM **OPI PSRAM**, Flash Size **16MB**, Partition Scheme **16M Flash (3MB APP/9.9MB FATFS)**.
5. Upload. If it won't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and try again.

### arduino-cli

```
arduino-cli core install esp32:esp32@3.3.12
arduino-cli lib install "LovyanGFX@1.2.33" "ESP8266Audio@2.4.1" "ESP8266SAM@1.1.0"
FQBN=esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile -b $FQBN CA53Y_Hiwonder        # add --build-property "compiler.cpp.extra_flags=-DLONG_RANGE=1" for Long Range
arduino-cli upload  -b $FQBN -p /dev/ttyUSB0 CA53Y_Hiwonder
```

### esptool (prebuilt file)

```
esptool --chip esp32s3 --port /dev/ttyUSB0 write-flash 0x0 docs/firmware/CA53Y_Hiwonder_S3.bin
```

Long Range (keeps saved settings):

```
cd docs/firmware/lr
esptool --chip esp32s3 --port /dev/ttyUSB0 write-flash 0x0 CA53Y_Hiwonder_S3_LR.bootloader.bin \
  0x8000 CA53Y_Hiwonder_S3_LR.partitions.bin 0xe000 boot_app0.bin 0x10000 CA53Y_Hiwonder_S3_LR.bin
```

## Using it

- **First boot:** type your name. A public channel is added for you.
- **Home:** Chats, Cameras, Settings along the bottom. Tap the clock or logo to open Chats. The status bar shows your name, unread count, Wi-Fi state and whether the mesh is running.
- **Add a contact:** Chats, then **+**, then *Add a contact by ID*, or tap someone under **Nearby**. Someone who messages you first is added automatically.
- **Encrypt a conversation:** open the chat, tap **Edit**, set a **Shared PIN**. They must set the same PIN for you.
- **Send:** in a chat, tap **Type** for the keyboard, or press and hold **Hold to talk**, speak, and let go.
- **Read aloud:** tap any received message, or **Play** on the pop-up. The voice is the classic robotic SAM synthesizer, which works with no internet.
- **Cameras:** Cameras, then **+**, or add them from the web settings page. In the viewer, tap for Back, Next, Home (show it on the home screen) and Remove.
- **BOOT button:** short press goes home or wakes the screen. Hold 5 seconds to erase everything (the device ID is kept).

### Wi-Fi and the mesh

All CA53Y devices talk on Wi-Fi channel 1, and the radio can only be on one channel at a time.

- If your router's 2.4 GHz network is on **channel 1**, the board stays on Wi-Fi and keeps relaying. Networks on channel 1 show in green in the Wi-Fi list.
- On **any other channel**, the board joins Wi-Fi only while it needs it: to set the clock (then every 6 hours), while a camera is on screen, while turning speech into text, or while you're on the Settings screens with Web settings opened. The rest of the time it's on channel 1. The status bar says **Mesh paused** while it's away.

A camera chosen as the home screen keeps Wi-Fi on whenever the home screen is showing. If your router isn't on channel 1, that board won't relay while it's showing the camera.

### Web settings

Settings shows the board's address on the **Web settings** row; tap it to see the 6-digit code (and to keep Wi-Fi up while you use the page). Open the address on a phone or computer on the same network, sign in with user **ca53y** and that code, and set:

- up to 6 cameras (name and address; for a camera with a login use `http://user:password@address/...`),
- which camera, if any, is the home screen,
- time zone and 12/24-hour clock,
- the speech-to-text server.

### Speech-to-text server

Hold to talk records up to 8 seconds, then POSTs it as a WAV file to any **OpenAI-compatible `/v1/audio/transcriptions` endpoint**, and shows you the words before sending. Options:

- **OpenAI:** address `https://api.openai.com/v1/audio/transcriptions`, your API key, model `whisper-1`.
- **Your own Whisper server** on your network (for example faster-whisper-server / Speaches, LocalAI, or whisper.cpp's `whisper-server` started with `--inference-path /v1/audio/transcriptions`): its `http://` address, no key, and the model name it expects.

The board doesn't verify HTTPS certificates (it has no certificate store), so a key sent to an `https://` server is encrypted but not protected against someone impersonating the server on your network. A local `http://` Whisper server avoids sending audio or keys off your network at all.

## Hardware notes

| Part                     | Pins                                                                 |
|--------------------------|----------------------------------------------------------------------|
| LCD ST7789 240x320 (SPI) | SCLK 21, MOSI 47, CS 2, DC 3; backlight on expander pin 14            |
| XL9555 I/O expander      | I2C SDA 38, SCL 48, address 0x20; pin 15 audio power                 |
| Touch (FT6336-style)     | I2C SDA 4, SCL 5, address 0x38                                       |
| ES8311 codec             | I2C SDA 4, SCL 5, address 0x18; I2S MCLK 45, BCLK 39, WS 41, DOUT 42, DIN 40 |
| BOOT button              | GPIO 0                                                               |

The pin map comes from a working XiaoZhi firmware port for this board (`HiwonderExploit_S3`). The board's other four buttons sit on the I/O expander and aren't used yet; the serial log (115200) prints `[xl9555]` lines when expander inputs change, which is how to map them.

## Troubleshooting

| Problem                          | Fix                                                                                          |
|----------------------------------|----------------------------------------------------------------------------------------------|
| Screen upside down               | Settings > Rotate screen (touch flips with it)                                               |
| Red and blue swapped             | Flip `c.rgb_order` in `board.h` and rebuild                                                  |
| Touch lands in the wrong place   | Note how it's off and open an issue; the mapping is in `touchRead()` in `board.h`            |
| No sound / mic too quiet         | Settings > Volume; raise `MIC_GAIN` in `config.h` (0 to 7)                                  |
| "Mesh paused" most of the time   | Set your router's 2.4 GHz channel to 1, or don't use a camera as the home screen            |
| Camera says "Needs a password"   | Use `http://user:password@address/...`                                                      |
| https camera                     | Use the camera's plain `http://` address; https streams aren't supported                     |
| Hold to talk says it needs a server | Add one under Web settings (see above)                                                    |

## Publishing the web flasher

The `docs/` folder is a ready-made flasher page. In the GitHub repo: Settings > Pages > Source **Deploy from a branch**, branch `main`, folder **/docs**. The page will be at `https://<your-user>.github.io/<repo>/`.

When you change the firmware, rebuild and replace `docs/firmware/CA53Y_Hiwonder_S3.bin`, or push a tag like `v1.0.1` and download the file the **Build firmware** action attaches to the release.

The packet format is in the display repo's [PROTOCOL.md](https://github.com/colinjcasey/CA53Y-Comms-Display/blob/main/PROTOCOL.md).

## License

The source in this repository is MIT. See [LICENSE](LICENSE). The CA53Y name and logo belong to CA53Y IND.

Read-aloud uses the [ESP8266SAM](https://github.com/earlephilhower/ESP8266SAM) library, which is installed separately and not part of this repository. Its wrapper is GPL-3.0 and the SAM speech engine inside it is reverse-engineered abandonware with no formal license; the prebuilt firmware in `docs/firmware` includes it.

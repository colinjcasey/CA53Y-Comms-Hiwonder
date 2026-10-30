/*
  CA53Y Comms for the Hiwonder ESP32-S3 Development Board (AI Vision & Smart Chat, 2" touchscreen).

  - CA53Y mesh messenger and repeater: direct messages (open or PIN-encrypted), channels, Nearby,
    delivery receipts. Works with the 2.8" Display, the dev-board messengers and the relays (protocol v2).
  - Home screen: the CA53Y IND logo, a clock once it's on Wi-Fi, or one of your cameras.
    New messages take over the screen with Play (read aloud) and Reply.
  - Hold to talk: your words become a text message through a speech-to-text server you choose.
  - Wi-Fi, cameras and the speech server are set up by the user, on the device or the web settings page.

  Build: Arduino-ESP32 core 3.x, board "ESP32S3 Dev Module", PSRAM "OPI PSRAM", Flash size 16MB,
  Partition scheme "16M Flash (3MB APP/9.9MB FATFS)". Libraries: LovyanGFX, ESP8266Audio, ESP8266SAM.
*/

#include "config.h"
#include "board.h"
#include "net.h"
#include "audio.h"
#include "online.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  loadState();
  loadOnline();
  boardSetup();
  uiSetup();          // screen and splash
  audioSetup();       // codec, speaker, microphone
  netSetup();         // ESP-NOW on channel 1
  delay(1200);
  if (!isSetup) openEditor(ED_MYNAME, "Welcome. Pick your name", "", 23, false, "Shown to people you message. You can change it in Settings.");
  else scr = SCR_HOME;
  needDraw = true;
  if (soundOn) chime();
}

void loop() {
  netLoop();
  wifiLoop();
  camLoop();
  uiLoop();
  delay(1);
}

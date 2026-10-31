// CA53Y Comms on the Hiwonder ESP32-S3 board: screens, keyboard, touch (landscape 320x240).
// Every screen is drawn into one full-screen sprite in PSRAM and pushed in one go, so nothing flickers.
#pragma once

// ---------- theme (same lacquer and gold as the other CA53Y devices) ----------
static constexpr uint32_t COL_LAC   = 0x0F2A30;
static constexpr uint32_t COL_LAC2  = 0x17393F;
static constexpr uint32_t COL_LINE  = 0x2D545A;
static constexpr uint32_t COL_GOLD  = 0xCFA84E;
static constexpr uint32_t COL_DGOLD = 0xA07A2C;
static constexpr uint32_t COL_CHAMP = 0xEAD9A8;
static constexpr uint32_t COL_IVORY = 0xF2EBD9;
static constexpr uint32_t COL_MIST  = 0xA3B8B2;
static constexpr uint32_t COL_VERD  = 0x86C2AE;
static constexpr uint32_t COL_ROSE  = 0xE39098;
static constexpr uint32_t COL_INK   = 0x1A2A2C;
static constexpr uint32_t COL_BLACK = 0x000000;

#define F_BODY  ((const lgfx::IFont *)&lgfx::fonts::FreeSans9pt7b)
#define F_SMALL ((const lgfx::IFont *)&lgfx::fonts::Font2)
#define F_TITLE ((const lgfx::IFont *)&lgfx::fonts::FreeSerif12pt7b)
#define F_LOGO  ((const lgfx::IFont *)&lgfx::fonts::FreeSerifBold24pt7b)
#define F_CLOCK ((const lgfx::IFont *)&lgfx::fonts::Font8)

using lgfx::textdatum_t;
#define D_TL textdatum_t::top_left
#define D_TC textdatum_t::top_center
#define D_TR textdatum_t::top_right
#define D_ML textdatum_t::middle_left
#define D_MC textdatum_t::middle_center
#define D_MR textdatum_t::middle_right
#define D_BL textdatum_t::baseline_left

static LGFX lcd;
static LGFX_Sprite spr(&lcd);
static LovyanGFX *G = &lcd;
#define W SCREEN_W
#define H SCREEN_H

static void fillR(int x, int y, int w, int h, uint32_t c) { G->fillRect(x, y, w, h, c); }
static void rfill(int x, int y, int w, int h, int r, uint32_t c) { G->fillRoundRect(x, y, w, h, r, c); }
static void rframe(int x, int y, int w, int h, int r, uint32_t c) { G->drawRoundRect(x, y, w, h, r, c); }
static void hline(int x, int y, int w, uint32_t c) { G->drawFastHLine(x, y, w, c); }
static void circ(int x, int y, int r, uint32_t c) { G->fillCircle(x, y, r, c); }
static void ring(int x, int y, int r, uint32_t c) { G->drawCircle(x, y, r, c); }
static void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) { G->fillTriangle(x0, y0, x1, y1, x2, y2, c); }
static void txt(const char *s, int x, int y, const lgfx::IFont *f, uint32_t c, textdatum_t d) {
  G->setFont(f);
  G->setTextDatum(d);
  G->setTextColor(c);
  G->drawString(s, x, y);
}

// ---------- text measuring and wrapping (the fonts are ASCII) ----------
static uint8_t cwBody[128], cwSmall[128];
static void initCharWidths() {
  char s[2] = {0, 0};
  for (int c = 0; c < 128; c++) {
    s[0] = (c < 32 || c > 126) ? '?' : c;
    lcd.setFont(F_BODY);  cwBody[c] = lcd.textWidth(s);
    lcd.setFont(F_SMALL); cwSmall[c] = lcd.textWidth(s);
  }
}
static inline int charW(const lgfx::IFont *f, char c) {
  uint8_t u = (uint8_t)c & 0x7F;
  return f == F_SMALL ? cwSmall[u] : cwBody[u];
}
static int strW(const char *s, const lgfx::IFont *f) { int w = 0; for (; *s; s++) w += charW(f, *s); return w; }

static void asciiize(char *dst, const char *src, size_t cap, bool flatten) {
  size_t j = 0;
  const uint8_t *p = (const uint8_t *)src;
  while (*p && j < cap - 1) {
    uint8_t c = *p;
    if (c < 0x80) { dst[j++] = (c == '\n' && flatten) ? ' ' : (char)c; p++; continue; }
    int n = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    char r = '?';
    if (n == 3 && p[0] == 0xE2 && p[1] == 0x80) {
      if (p[2] == 0x98 || p[2] == 0x99) r = '\'';
      else if (p[2] == 0x9C || p[2] == 0x9D) r = '"';
      else if (p[2] == 0x93 || p[2] == 0x94) r = '-';
    }
    dst[j++] = r;
    for (int k = 0; k < n && *p; k++) p++;
  }
  dst[j] = 0;
}

static String fitText(const char *s, int maxW, const lgfx::IFont *f) {
  char a[200];
  asciiize(a, s, sizeof a, true);
  if (strW(a, f) <= maxW) return String(a);
  int dots = charW(f, '.') * 3, w = 0, i = 0;
  for (; a[i]; i++) { int c = charW(f, a[i]); if (w + c + dots > maxW) break; w += c; }
  a[i] = 0;
  return String(a) + "...";
}

static int wrapText(const char *s, int maxW, const lgfx::IFont *f, uint16_t *ls, uint8_t *ll, int maxL) {
  int len = strlen(s), n = 0, i = 0;
  while (i < len && n < maxL) {
    int start = i, lastSpace = -1, w = 0, j = i;
    while (j < len && s[j] != '\n') {
      int c = charW(f, s[j]);
      if (w + c > maxW) break;
      if (s[j] == ' ') lastSpace = j;
      w += c;
      j++;
    }
    if (j >= len || s[j] == '\n') { ls[n] = start; ll[n] = j - start; n++; i = j + 1; continue; }
    if (lastSpace > start) { ls[n] = start; ll[n] = lastSpace - start; i = lastSpace + 1; }
    else { int e = j > start ? j : start + 1; ls[n] = start; ll[n] = e - start; i = e; }
    n++;
  }
  if (n == 0) { ls[0] = 0; ll[0] = 0; n = 1; }
  return n;
}

static void drawLines(const char *s, const uint16_t *ls, const uint8_t *ll, int n, int x, int y, int step,
                      const lgfx::IFont *f, uint32_t c) {
  char line[200];
  for (int i = 0; i < n; i++) {
    int k = min((int)ll[i], (int)sizeof line - 1);
    memcpy(line, s + ls[i], k);
    line[k] = 0;
    txt(line, x, y + i * step, f, c, D_TL);
  }
}

static int paragraph(const char *s, int x, int y, int w, const lgfx::IFont *f, uint32_t c, int step, int maxLines = 10) {
  char a[400];
  asciiize(a, s, sizeof a, false);
  uint16_t ls[12]; uint8_t ll[12];
  int n = wrapText(a, w, f, ls, ll, min(maxLines, 12));
  drawLines(a, ls, ll, n, x, y, step, f, c);
  return n;
}

// ---------- ornaments ----------
static void lockIcon(int x, int y, uint32_t c) {
  rframe(x + 2, y, 6, 8, 3, c);
  rframe(x + 3, y + 1, 4, 7, 2, c);
  fillR(x, y + 5, 10, 7, c);
}
static void badge(int xr, int y, int n) {
  char b[6];
  snprintf(b, sizeof b, "%d", n > 99 ? 99 : n);
  int w = max(20, strW(b, F_SMALL) + 12);
  rfill(xr - w, y - 10, w, 20, 10, COL_GOLD);
  txt(b, xr - w / 2, y, F_SMALL, COL_INK, D_MC);
}
static void signalBars(int x, int y, int rssi) {
  int bars = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
  for (int i = 0; i < 4; i++) fillR(x + i * 5, y - 4 - i * 3, 3, 4 + i * 3, i < bars ? COL_VERD : COL_LINE);
}
static void decoRule(int y) {
  hline(0, y, W, COL_GOLD);
  hline(0, y + 2, W, COL_LINE);
  tri(W / 2, y - 3, W / 2 + 4, y + 1, W / 2 - 4, y + 1, COL_GOLD);
  tri(W / 2 - 4, y + 1, W / 2 + 4, y + 1, W / 2, y + 5, COL_GOLD);
}
static void plate(const char *s, int cx, int y, uint32_t c, uint32_t bg) {
  int w = strW(s, F_SMALL) + 18;
  int x = cx - w / 2;
  fillR(x, y - 11, w, 22, bg);
  rframe(x, y - 11, w, 22, 0, c);
  fillR(x, y - 11, 3, 22, c);
  fillR(x + w - 3, y - 11, 3, 22, c);
  txt(s, cx, y, F_SMALL, c, D_MC);
}
static void button(int x, int y, int w, int h, const char *label, bool solid, uint32_t c) {
  if (solid) rfill(x, y, w, h, 3, c);
  else { rfill(x, y, w, h, 3, COL_LAC); rframe(x, y, w, h, 3, c); }
  txt(label, x + w / 2, y + h / 2, F_SMALL, solid ? COL_INK : c, D_MC);
}
static inline bool in(int x, int y, int bx, int by, int bw, int bh) { return x >= bx && x < bx + bw && y >= by && y < by + bh; }

// The CA53Y IND sunburst mark, drawn rather than stored. tools/make_logo.py swaps in your own artwork.
#if __has_include("logo.h")
#include "logo.h"
#define HAS_LOGO 1
#else
#define HAS_LOGO 0
#endif

static void sunburst(int cx, int cy, int r0, int r1, uint32_t c) {
  for (int a = 0; a < 360; a += 10) {
    float t0 = (a - 2.2f) * DEG_TO_RAD, t1 = (a + 2.2f) * DEG_TO_RAD, tm = a * DEG_TO_RAD;
    tri(cx + cosf(t0) * r0, cy + sinf(t0) * r0, cx + cosf(t1) * r0, cy + sinf(t1) * r0,
        cx + cosf(tm) * r1, cy + sinf(tm) * r1, c);
  }
}

static void emblem(int cx, int cy, float s, uint32_t bg) {
#if HAS_LOGO
  (void)s; (void)bg;
  G->pushImage(cx - CA53Y_LOGO_W / 2, cy - CA53Y_LOGO_H / 2, CA53Y_LOGO_W, CA53Y_LOGO_H, (const lgfx::swap565_t *)ca53y_logo);
#else
  int r0 = 64 * s, r1 = 88 * s;
  sunburst(cx, cy, r0, r1, COL_GOLD);
  circ(cx, cy, r0, bg);
  ring(cx, cy, r0, COL_GOLD);
  ring(cx, cy, r0 - 4, COL_DGOLD);
  G->setTextSize(0.85f * s);
  txt("CA53Y", cx + 1, cy - 3, F_LOGO, COL_DGOLD, D_MC);   // gold with a darker edge, like stamped foil
  txt("CA53Y", cx, cy - 4, F_LOGO, COL_GOLD, D_MC);
  G->setTextSize(1);
  int w = 40 * s, y = cy + 18 * s;
  fillR(cx - w / 2, y, w, 16 * s, COL_GOLD);
  if (s >= 0.8f) txt("IND", cx, y + 8, F_SMALL, bg, D_MC);
#endif
}

// ---------- UI state ----------
enum : uint8_t { SCR_HOME, SCR_ALERT, SCR_LIST, SCR_THREAD, SCR_KEYS, SCR_VOICE, SCR_SETTINGS, SCR_WIFI,
                 SCR_CAMS, SCR_CAMVIEW, SCR_PICK, SCR_CONTACT, SCR_CHAN, SCR_CONFIRM, SCR_BUSY };
enum : uint8_t { ED_MSG, ED_MYNAME, ED_C_ID, ED_C_NICK, ED_C_PIN, ED_CH_NAME, ED_CH_PASS, ED_W_SSID, ED_W_PASS,
                 ED_CAM_NAME, ED_CAM_URL, ED_VOICE };
enum : uint8_t { PK_ADD, PK_TZ, PK_HOME, PK_VOL, PK_SLEEP };
enum : uint8_t { CONF_ERASE, CONF_DEL_CONTACT, CONF_LEAVE_CHAN, CONF_DEL_CAM };

#define BAR_H 30      // title bar
#define DOCK_Y 200    // bottom buttons
#define ROW_H 42

uint8_t scr = SCR_HOME, listTab = 0;
bool needDraw = true;
int scrollY = 0, scrollMax = 0;

// thread
uint8_t thKind = 0;
char thPeer[20];
bool thStick = true;

// alert (incoming message)
uint32_t alertUid = 0;

// keyboard
struct Editor { uint8_t target, layer, ret; bool shift, mask; int maxLen; char buf[200]; char title[40]; char hint[90]; } ed;

// forms
struct { bool editing; char id[20], nick[24], pin[33]; uint8_t pinMode; bool hadPin; } cf;   // pinMode 0 keep, 1 set, 2 clear
struct { int idx; char name[24], pass[33]; } chf;
struct { char name[24]; } camNew;
uint8_t confirmAction = 0;
int confirmIdx = -1;
uint8_t pickKind = 0;
uint8_t wifiPick = 0;
int scanCount = -1;
char wPickSsid[33];

// voice
char voiceText[MAX_TEXT + 40];
uint8_t voiceRet = SCR_THREAD;

// camera viewer
bool camOverlay = true;
uint32_t camOverlayAt = 0;

// toast
struct { bool on; uint32_t until; char text[80]; uint32_t color; } toast;

// screen power
bool asleep = false, autoRead = false;
uint32_t lastInput = 0;
static const uint32_t SLEEP_MS[4] = {60000, 300000, 1800000, 0};
static const char *SLEEP_LBL[4] = {"1 minute", "5 minutes", "30 minutes", "Never"};

static void showNote(const char *text, uint32_t color) {
  toast.on = true;
  toast.until = millis() + 2800;
  strlcpy(toast.text, text, sizeof toast.text);
  toast.color = color;
  needDraw = true;
}

// ---------- names ----------
static const char *contactName(int ci) { return contacts[ci].nick[0] ? contacts[ci].nick : contacts[ci].id + PREFIX_LEN; }
static String senderName(const Msg *m) {
  if (m->dir == 1) return String("You");
  int ci = findContact(m->from);
  if (ci >= 0 && contacts[ci].nick[0]) return String(contacts[ci].nick);
  if (m->fromNick[0]) return String(m->fromNick);
  return String(m->from + PREFIX_LEN);
}
static String peerTitle(uint8_t kind, const char *peer) {
  if (kind == 0) { int ci = findContact(peer); return ci >= 0 ? String(contactName(ci)) : String(peer + PREFIX_LEN); }
  int chi = findChanByAddr(peer);
  return chi >= 0 ? String("#") + chans[chi].name : String("#?");
}
static Msg *lastMsgFor(const char *peer) {
  for (int i = msgCount - 1; i >= 0; i--) { Msg *m = msgAt(i); if (m->st != 99 && !strcasecmp(m->peer, peer)) return m; }
  return nullptr;
}
static int totalUnread() {
  int n = 0;
  for (int i = 0; i < nContacts; i++) n += crt[i].unread;
  for (int i = 0; i < nChans; i++) n += chrt[i].unread;
  return n;
}

// ---------- status bar ----------
static void statusBar(bool overlay) {
  if (!overlay) fillR(0, 0, W, 24, COL_LAC);
  else { G->fillRect(0, 0, W, 24, COL_BLACK); }
  txt(fitText(myNick[0] ? myNick : myId, 120, F_SMALL).c_str(), 8, 12, F_SMALL, COL_CHAMP, D_ML);
  int x = W - 8;
  int un = totalUnread();
  if (un) { badge(x, 12, un); x -= 30; }
  const char *ws = nullptr; uint32_t wc = COL_MIST;
  if (wifiOn) {
    if (wState == W_UP) { ws = "Wi-Fi"; wc = COL_VERD; }
    else if (wState == W_JOINING) { ws = "Joining"; wc = COL_GOLD; }
    else if (wErr[0]) { ws = "Wi-Fi off"; wc = COL_ROSE; }
  }
  if (ws) { txt(ws, x, 12, F_SMALL, wc, D_MR); x -= strW(ws, F_SMALL) + 12; }
  if (meshPaused()) txt("Mesh paused", x, 12, F_SMALL, COL_ROSE, D_MR);
  else txt(relayOn ? "Mesh" : "Mesh (no repeat)", x, 12, F_SMALL, COL_VERD, D_MR);
}

static void titleBar(const char *title, bool back, const char *right) {
  fillR(0, 0, W, BAR_H, COL_LAC);
  hline(0, BAR_H - 1, W, COL_LINE);
  if (back) { tri(12, 15, 20, 8, 20, 22, COL_GOLD); }
  txt(fitText(title, 200, F_BODY).c_str(), back ? 30 : 10, 15, F_BODY, COL_CHAMP, D_ML);
  if (right) txt(right, W - 10, 15, F_BODY, COL_GOLD, D_MR);
}

static void dock(const char **labels, int n, int highlight) {
  int bw = (W - 8 - (n - 1) * 6) / n;
  for (int i = 0; i < n; i++) button(4 + i * (bw + 6), DOCK_Y + 4, bw, H - DOCK_Y - 8, labels[i], i == highlight, COL_GOLD);
}
static int dockHit(int x, int y, int n) {
  if (y < DOCK_Y) return -1;
  int bw = (W - 8 - (n - 1) * 6) / n;
  for (int i = 0; i < n; i++) if (x >= 4 + i * (bw + 6) && x < 4 + i * (bw + 6) + bw) return i;
  return -1;
}

// ---------- home ----------
static void drawCamFrame() {
  int w = 0, h = 0;
  if (camReady && camLen && jpegSize(camBuf, camLen, w, h)) {
    float sc = min((float)W / w, (float)H / h);
    G->fillScreen(COL_BLACK);
    G->drawJpg(camBuf, camLen, W / 2, H / 2, W, H, 0, 0, sc, sc, lgfx::datum_t::middle_center);
  } else {
    fillR(0, 0, W, H, COL_LAC);
    const char *m = !wifiOn ? "Join Wi-Fi in Settings to see cameras"
                  : camErr[0] ? camErr
                  : wState != W_UP ? "Joining Wi-Fi..." : "Connecting to the camera...";
    paragraph(m, 30, 104, W - 60, F_BODY, camErr[0] ? COL_ROSE : COL_MIST, 20, 3);
  }
}

static void drawClockBlock(int cy, bool onCam) {
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  char hm[8], date[40];
  int hr = tm.tm_hour;
  if (!h24) { hr %= 12; if (!hr) hr = 12; }
  snprintf(hm, sizeof hm, h24 ? "%02d:%02d" : "%d:%02d", hr, tm.tm_min);
  strftime(date, sizeof date, "%A, %B %e", &tm);
  if (onCam) {
    G->setFont(F_BODY);
    int w = G->textWidth(hm) + 16;
    rfill(W - w - 6, 30, w, 24, 4, COL_BLACK);
    txt(hm, W - 14, 42, F_BODY, COL_CHAMP, D_MR);
    return;
  }
  txt(hm, W / 2, cy, F_CLOCK, COL_GOLD, D_MC);
  txt(date, W / 2, cy + 58, F_TITLE, COL_IVORY, D_MC);
}

static void drawHome() {
  bool camHome = homeCam >= 0 && homeCam < nCams && wifiOn;
  if (camHome) {
    drawCamFrame();
    statusBar(true);
    if (timeValid()) drawClockBlock(0, true);
  } else {
    fillR(0, 0, W, H, COL_LAC);
    statusBar(false);
    if (timeValid()) {
      plate("CA53Y  IND", W / 2, 44, COL_GOLD, COL_LAC);
      drawClockBlock(106, false);
    } else {
      emblem(W / 2, 112, 1.0f, COL_LAC);
    }
  }
  const char *labels[3] = {"Chats", "Cameras", "Settings"};
  dock(labels, 3, -1);
}

// ---------- incoming message ----------
static void drawAlert() {
  Msg *m = findMsg(alertUid);
  fillR(0, 0, W, H, COL_LAC);
  if (!m) return;
  fillR(0, 0, W, 34, COL_LAC2);
  txt(m->kind ? "Channel message" : "New message", 10, 17, F_SMALL, COL_MIST, D_ML);
  if (m->kind) txt(fitText(peerTitle(1, m->peer).c_str(), 150, F_SMALL).c_str(), W - 10, 17, F_SMALL, COL_GOLD, D_MR);
  txt(fitText(senderName(m).c_str(), W - 20, F_TITLE).c_str(), 10, 52, F_TITLE, COL_CHAMP, D_ML);
  decoRule(70);
  if (m->st == 11) paragraph("Locked: their PIN for you doesn't match yours. Check the Shared PIN on both devices.", 12, 84, W - 24, F_BODY, COL_ROSE, 20, 5);
  else paragraph(m->text, 12, 84, W - 24, F_BODY, COL_IVORY, 20, 5);
  const char *labels[4] = {"Play", "Reply", "Open", "Close"};
  dock(labels, 4, 0);
}

// ---------- chats / nearby ----------
struct Row { uint8_t kind; int idx; uint32_t act; };
static int buildChatRows(Row *rows) {
  int n = 0;
  for (int i = 0; i < nContacts; i++) rows[n++] = {0, i, crt[i].lastAct};
  for (int i = 0; i < nChans; i++) rows[n++] = {1, i, chrt[i].lastAct};
  for (int i = 1; i < n; i++) for (int j = i; j > 0 && rows[j].act > rows[j - 1].act; j--) { Row t = rows[j]; rows[j] = rows[j - 1]; rows[j - 1] = t; }
  return n;
}
static int nearbyList(Nearby **out) {
  int n = 0;
  uint32_t now = millis();
  for (auto &nb : nearby) if (nb.used && now - nb.seen <= NEARBY_TTL_MS) out[n++] = &nb;
  for (int i = 1; i < n; i++) for (int j = i; j > 0 && out[j]->rssi > out[j - 1]->rssi; j--) { Nearby *t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
  return n;
}

static void listRow(int y, const char *title, const String &sub, uint32_t tc, int unread, bool lock, int rssi) {
  if (y + ROW_H <= BAR_H || y >= DOCK_Y) return;
  txt(fitText(title, 210, F_BODY).c_str(), 12, y + 13, F_BODY, tc, D_ML);
  if (lock) lockIcon(14 + strW(fitText(title, 210, F_BODY).c_str(), F_BODY) + 6, y + 7, COL_GOLD);
  txt(fitText(sub.c_str(), 250, F_SMALL).c_str(), 12, y + 31, F_SMALL, COL_MIST, D_ML);
  if (unread) badge(W - 12, y + 21, unread);
  if (rssi) signalBars(W - 34, y + 30, rssi);
  hline(12, y + ROW_H - 1, W - 24, COL_LINE);
}

static void drawList() {
  fillR(0, 0, W, H, COL_LAC);
  int y0 = BAR_H - scrollY;
  int content = 0;
  if (listTab == 0) {
    Row rows[MAX_CONTACTS + MAX_CHANNELS];
    int n = buildChatRows(rows);
    for (int i = 0; i < n; i++) {
      int y = y0 + i * ROW_H;
      if (rows[i].kind == 0) {
        int ci = rows[i].idx;
        Msg *m = lastMsgFor(contacts[ci].id);
        String sub = m ? (m->dir ? String("You: ") : String("")) + (m->st == 11 ? String("(locked)") : String(m->text))
                       : String("CA53Ycomms-") + (contacts[ci].id + PREFIX_LEN);
        listRow(y, contactName(ci), sub, crt[ci].isNew ? COL_GOLD : COL_IVORY, crt[ci].unread, contacts[ci].pin[0], 0);
      } else {
        int chi = rows[i].idx;
        Msg *m = lastMsgFor(chrt[chi].addr);
        String sub = m ? senderName(m) + ": " + m->text : String(chans[chi].pass[0] ? "Private channel" : "Open channel");
        listRow(y, (String("#") + chans[chi].name).c_str(), sub, COL_IVORY, chrt[chi].unread, chans[chi].pass[0], 0);
      }
    }
    content = n * ROW_H;
    if (!n) paragraph("No chats yet. Tap + to add someone by their device ID, or Nearby to see who's in range.", 20, 70, W - 40, F_BODY, COL_MIST, 20);
  } else {
    Nearby *nb[MAX_NEARBY];
    int n = nearbyList(nb);
    for (int i = 0; i < n; i++) {
      bool known = findContact(nb[i]->id) >= 0;
      listRow(y0 + i * ROW_H, nb[i]->nick[0] ? nb[i]->nick : nb[i]->id + PREFIX_LEN,
              String(nb[i]->id) + (known ? "  (contact)" : "  tap to add"), COL_IVORY, 0, false, nb[i]->rssi);
    }
    content = n * ROW_H;
    if (!n) paragraph("Nobody in direct range right now. Devices further away can still be reached by ID through repeaters.", 20, 70, W - 40, F_BODY, COL_MIST, 20);
  }
  scrollMax = max(0, content - (DOCK_Y - BAR_H));
  titleBar(listTab == 0 ? "Chats" : "Nearby", true, "+");
  fillR(0, DOCK_Y, W, H - DOCK_Y, COL_LAC);
  hline(0, DOCK_Y, W, COL_LINE);
  const char *labels[2] = {"Chats", "Nearby"};
  dock(labels, 2, listTab);
}

// ---------- thread ----------
#define TH_BOT 200
struct TItem { uint32_t uid; int y, h, bw; uint8_t nl; uint16_t ls[8]; uint8_t ll[8]; bool label; };
TItem titems[MAX_MSGS];
int tcount = 0, tContentH = 0;
char tAscii[MAX_MSGS][MAX_TEXT + 4];

static void layoutThread() {
  tcount = 0;
  int y = 6;
  const char *lastFrom = "";
  for (int i = 0; i < msgCount; i++) {
    Msg *m = msgAt(i);
    if (m->st == 99 || strcasecmp(m->peer, thPeer)) continue;
    TItem &t = titems[tcount];
    asciiize(tAscii[tcount], m->st == 11 ? "Locked: PIN mismatch" : m->text, sizeof tAscii[0], false);
    t.uid = m->uid;
    t.label = thKind == 1 && m->dir == 0 && strcasecmp(lastFrom, m->from);
    lastFrom = m->dir ? "" : m->from;
    t.nl = wrapText(tAscii[tcount], 220, F_BODY, t.ls, t.ll, 8);
    int maxw = 0;
    for (int k = 0; k < t.nl; k++) {
      char line[200]; memcpy(line, tAscii[tcount] + t.ls[k], t.ll[k]); line[t.ll[k]] = 0;
      maxw = max(maxw, strW(line, F_BODY));
    }
    t.bw = maxw + 20;
    t.y = y;
    t.h = t.nl * 19 + 12 + (t.label ? 16 : 0) + (m->dir ? 14 : 0);
    y += t.h + 6;
    tcount++;
  }
  tContentH = y;
  scrollMax = max(0, tContentH - (TH_BOT - BAR_H));
  if (thStick) scrollY = scrollMax;
  scrollY = constrain(scrollY, 0, scrollMax);
}

static const char *statusLabel(const Msg *m) {
  switch (m->st) {
    case 0: return "Sending...";
    case 1: return "Delivered";
    case 2: return "Not delivered - tap to resend";
    case 3: return "Delivered, but their PIN doesn't match";
    case 4: return "Sent";
    default: return "";
  }
}

static void drawThread() {
  layoutThread();
  fillR(0, 0, W, H, COL_LAC);
  int base = BAR_H - scrollY;
  for (int i = 0; i < tcount; i++) {
    TItem &t = titems[i];
    int y = base + t.y;
    if (y + t.h < BAR_H || y > TH_BOT) continue;
    Msg *m = findMsg(t.uid);
    if (!m) continue;
    bool out = m->dir == 1;
    int x = out ? W - 10 - t.bw : 10;
    int by = y;
    if (t.label) { txt(fitText(senderName(m).c_str(), 200, F_SMALL).c_str(), x + 2, y + 6, F_SMALL, COL_GOLD, D_ML); by += 16; }
    int bh = t.nl * 19 + 12;
    rfill(x, by, t.bw, bh, 8, out ? COL_GOLD : COL_LAC2);
    if (!out) G->drawRoundRect(x, by, t.bw, bh, 8, COL_LINE);
    drawLines(tAscii[i], t.ls, t.ll, t.nl, x + 10, by + 6, 19, F_BODY, out ? COL_INK : (m->st == 11 ? COL_ROSE : COL_IVORY));
    if (out) txt(statusLabel(m), W - 12, by + bh + 7, F_SMALL, m->st == 2 || m->st == 3 ? COL_ROSE : COL_MIST, D_MR);
  }
  if (!tcount) paragraph(thKind ? "No messages in this channel yet." : "No messages yet. Say hello.", 20, 90, W - 40, F_BODY, COL_MIST, 20);
  String title = peerTitle(thKind, thPeer);
  bool locked = thKind == 0 ? (findContact(thPeer) >= 0 && contacts[findContact(thPeer)].pin[0])
                            : (findChanByAddr(thPeer) >= 0 && chans[findChanByAddr(thPeer)].pass[0]);
  titleBar(title.c_str(), true, "Edit");
  if (locked) lockIcon(30 + strW(fitText(title.c_str(), 200, F_BODY).c_str(), F_BODY) + 6, 9, COL_GOLD);
  fillR(0, TH_BOT, W, H - TH_BOT, COL_LAC);
  hline(0, TH_BOT, W, COL_LINE);
  const char *labels[2] = {"Type", "Hold to talk"};
  dock(labels, 2, 1);
}

static void openThread(uint8_t kind, const char *peer) {
  thKind = kind;
  strlcpy(thPeer, peer, sizeof thPeer);
  thStick = true;
  if (kind == 0) { int ci = findContact(peer); if (ci >= 0) { crt[ci].unread = 0; crt[ci].isNew = false; } }
  else { int chi = findChanByAddr(peer); if (chi >= 0) chrt[chi].unread = 0; }
  scr = SCR_THREAD;
  needDraw = true;
}

// ---------- keyboard ----------
struct Key { int16_t x, y, w, h; char c; uint8_t code; };   // code: 0 char, 1 shift, 2 backspace, 3 layer, 4 done, 5 space
#define KB_Y 72
#define KB_RH 42

static int buildKeys(Key *k) {
  int n = 0;
  auto row = [&](int r, const char *s, int x0, int kw) {
    for (int i = 0; s[i]; i++) k[n++] = {(int16_t)(x0 + i * kw), (int16_t)(KB_Y + r * KB_RH), (int16_t)kw, (int16_t)KB_RH, s[i], 0};
  };
  if (ed.layer == 0) {
    row(0, "qwertyuiop", 0, 32);
    row(1, "asdfghjkl", 16, 32);
    k[n++] = {0, KB_Y + 2 * KB_RH, 48, KB_RH, 0, 1};
    row(2, "zxcvbnm", 48, 32);
  } else {
    row(0, "1234567890", 0, 32);
    row(1, "-/:;()&@\"", 16, 32);
    k[n++] = {0, KB_Y + 2 * KB_RH, 48, KB_RH, 0, 1};
    row(2, ".,?!'#+", 48, 32);
  }
  k[n++] = {272, KB_Y + 2 * KB_RH, 48, KB_RH, 0, 2};
  k[n++] = {0, KB_Y + 3 * KB_RH, 56, KB_RH, 0, 3};
  k[n++] = {56, KB_Y + 3 * KB_RH, 160, KB_RH, ' ', 5};
  k[n++] = {216, KB_Y + 3 * KB_RH, 32, KB_RH, ed.layer ? '_' : '.', 0};
  k[n++] = {248, KB_Y + 3 * KB_RH, 72, KB_RH, 0, 4};
  return n;
}

static void drawKeys() {
  fillR(0, 0, W, H, COL_LAC);
  txt(ed.title, 10, 10, F_SMALL, COL_MIST, D_ML);
  txt("Cancel", W - 10, 10, F_SMALL, COL_GOLD, D_MR);
  rfill(6, 22, W - 12, 44, 3, COL_LAC2);
  rframe(6, 22, W - 12, 44, 3, COL_LINE);
  char shown[220];
  if (ed.mask) { int L = strlen(ed.buf); for (int i = 0; i < L; i++) shown[i] = '*'; shown[L] = 0; }
  else asciiize(shown, ed.buf, sizeof shown, true);
  if (!shown[0]) paragraph(ed.hint, 12, 26, W - 24, F_SMALL, COL_MIST, 16, 2);
  else {
    uint16_t ls[12]; uint8_t ll[12];
    int nl = wrapText(shown, W - 30, F_BODY, ls, ll, 12);
    int first = max(0, nl - 2);
    drawLines(shown, ls + first, ll + first, nl - first, 12, 25, 19, F_BODY, COL_IVORY);
    char last[200];
    int k = ll[nl - 1]; memcpy(last, shown + ls[nl - 1], k); last[k] = 0;
    int cx = 12 + strW(last, F_BODY), cy = 25 + (nl - first - 1) * 19;
    if ((millis() / 500) & 1) fillR(cx + 1, cy + 2, 2, 15, COL_GOLD);
  }
  char cnt[16];
  snprintf(cnt, sizeof cnt, "%d/%d", (int)strlen(ed.buf), ed.maxLen);
  txt(cnt, W - 12, 60, F_SMALL, COL_MIST, D_MR);
  Key k[40];
  int n = buildKeys(k);
  for (int i = 0; i < n; i++) {
    Key &q = k[i];
    bool special = q.code == 1 || q.code == 2 || q.code == 3 || q.code == 4;
    uint32_t bg = q.code == 4 ? COL_GOLD : special ? COL_LINE : COL_LAC2;
    rfill(q.x + 2, q.y + 3, q.w - 4, q.h - 6, 4, bg);
    char lab[8] = {0};
    if (q.code == 0) { lab[0] = (ed.shift && q.c >= 'a' && q.c <= 'z') ? q.c - 32 : q.c; }
    else if (q.code == 1) strcpy(lab, ed.shift ? "SHIFT" : "shift");
    else if (q.code == 2) strcpy(lab, "<x");
    else if (q.code == 3) strcpy(lab, ed.layer ? "ABC" : "123");
    else if (q.code == 4) strcpy(lab, "Done");
    else if (q.code == 5) strcpy(lab, "space");
    txt(lab, q.x + q.w / 2, q.y + q.h / 2, q.code == 0 ? F_BODY : F_SMALL, q.code == 4 ? COL_INK : COL_IVORY, D_MC);
  }
}

static void openEditor(uint8_t target, const char *title, const char *initial, int maxLen, bool mask, const char *hint) {
  ed.target = target;
  ed.ret = scr;
  ed.layer = 0;
  ed.shift = target == ED_MSG || target == ED_MYNAME || target == ED_C_NICK || target == ED_CAM_NAME;
  ed.mask = mask;
  ed.maxLen = min(maxLen, (int)sizeof ed.buf - 1);
  strlcpy(ed.buf, initial ? initial : "", sizeof ed.buf);
  strlcpy(ed.title, title, sizeof ed.title);
  strlcpy(ed.hint, hint ? hint : "", sizeof ed.hint);
  scr = SCR_KEYS;
  needDraw = true;
}

void sendText(const char *text) {
  const char *err = nullptr;
  if (thKind == 0) err = sendDirect(findContact(thPeer), text);
  else err = sendChannel(findChanByAddr(thPeer), text);
  if (err) showNote(err, COL_ROSE);
  thStick = true;
}

static void openPick(uint8_t kind);
static void startWifiScan();

static void editorDone() {
  String v(ed.buf);
  if (ed.target != ED_MSG && ed.target != ED_VOICE && ed.target != ED_W_PASS) v.trim();
  uint8_t back = ed.ret;
  scr = back;
  needDraw = true;
  switch (ed.target) {
    case ED_MSG:
    case ED_VOICE:
      if (v.length()) sendText(v.c_str());
      scr = SCR_THREAD;
      break;
    case ED_MYNAME:
      if (!v.length()) { showNote("Pick a name others will see.", COL_ROSE); scr = SCR_KEYS; return; }
      strlcpy(myNick, v.c_str(), sizeof myNick);
      prefs.putString("nick", myNick);
      if (!isSetup) {
        isSetup = true;
        prefs.putBool("setup", true);
        if (nChans == 0) { addChannel("Public", ""); saveChannels(); }
        scr = SCR_HOME;
      }
      break;
    case ED_C_ID: strlcpy(cf.id, v.c_str(), sizeof cf.id); break;
    case ED_C_NICK: strlcpy(cf.nick, v.c_str(), sizeof cf.nick); break;
    case ED_C_PIN:
      if (v.length() && v.length() < 4) { showNote("PINs are at least 4 characters.", COL_ROSE); scr = SCR_KEYS; return; }
      strlcpy(cf.pin, v.c_str(), sizeof cf.pin);
      cf.pinMode = v.length() ? 1 : (cf.hadPin ? 2 : 0);
      break;
    case ED_CH_NAME: strlcpy(chf.name, v.c_str(), sizeof chf.name); break;
    case ED_CH_PASS: strlcpy(chf.pass, v.c_str(), sizeof chf.pass); break;
    case ED_W_SSID:
      if (!v.length()) return;
      strlcpy(wPickSsid, v.c_str(), sizeof wPickSsid);
      openEditor(ED_W_PASS, "Wi-Fi password", "", 63, true, "Leave empty for an open network.");
      ed.ret = SCR_WIFI;
      return;
    case ED_W_PASS:
      wifiSet(wPickSsid, ed.buf);
      showNote("Joining Wi-Fi...", COL_GOLD);
      scr = SCR_SETTINGS;
      break;
    case ED_CAM_NAME:
      strlcpy(camNew.name, v.length() ? v.c_str() : "Camera", sizeof camNew.name);
      openEditor(ED_CAM_URL, "Camera address", "http://", 159, false, "MJPEG stream or JPEG snapshot, e.g. http://192.168.1.50:81/stream");
      ed.ret = SCR_CAMS;
      return;
    case ED_CAM_URL: {
      Url u;
      if (!parseUrl(v.c_str(), u)) { showNote("Use an http:// address", COL_ROSE); scr = SCR_KEYS; return; }
      if (nCams >= MAX_CAMS) { showNote("Camera list is full", COL_ROSE); break; }
      strlcpy(cams[nCams].name, camNew.name, sizeof cams[nCams].name);
      strlcpy(cams[nCams].url, v.c_str(), sizeof cams[nCams].url);
      nCams++;
      saveCams();
      scr = SCR_CAMS;
      break;
    }
  }
}

static void keyTap(int x, int y) {
  if (y < 20 && x > W - 70) { scr = ed.ret; if (!isSetup) scr = SCR_KEYS; needDraw = true; return; }
  Key k[40];
  int n = buildKeys(k);
  for (int i = 0; i < n; i++) {
    Key &q = k[i];
    if (!in(x, y, q.x, q.y, q.w, q.h)) continue;
    int L = strlen(ed.buf);
    switch (q.code) {
      case 0: case 5:
        if (L < ed.maxLen) {
          char c = q.c;
          if (ed.shift && c >= 'a' && c <= 'z') c -= 32;
          ed.buf[L] = c; ed.buf[L + 1] = 0;
          if (ed.shift && q.code == 0 && ed.layer == 0) ed.shift = false;
        }
        break;
      case 1: ed.shift = !ed.shift; break;
      case 2:
        if (L) { L--; while (L > 0 && ((uint8_t)ed.buf[L] & 0xC0) == 0x80) L--; ed.buf[L] = 0; }
        break;
      case 3: ed.layer ^= 1; break;
      case 4: editorDone(); return;
    }
    needDraw = true;
    return;
  }
}

// ---------- hold to talk ----------
static void pushNow() { spr.pushSprite(0, 0); }

static void busyScreen(const char *msg) {
  G = &spr;
  fillR(0, 0, W, H, COL_LAC);
  emblem(W / 2, 78, 0.7f, COL_LAC);
  paragraph(msg, 30, 150, W - 60, F_BODY, COL_CHAMP, 20, 3);
  pushNow();
}

bool sayAbort() { int x, y; return touchRead(x, y); }

void runTalk() {
  if (!audioOk) { showNote("The microphone isn't working.", COL_ROSE); return; }
  if (!recBegin()) { showNote("Not enough memory to record.", COL_ROSE); return; }
  uint32_t t0 = millis(), lastUi = 0, lastTouch = millis();
  int x, y;
  while (true) {
    bool more = recPoll();
    uint32_t now = millis();
    if (touchRead(x, y)) lastTouch = now;
    if (now - lastTouch > 150 || !more) break;
    if (now - lastUi > 120) {
      lastUi = now;
      G = &spr;
      fillR(0, 0, W, H, COL_LAC);
      circ(W / 2, 80, 34, COL_ROSE);
      rfill(W / 2 - 9, 58, 18, 30, 9, COL_LAC);
      fillR(W / 2 - 1, 92, 2, 10, COL_LAC);
      txt("Listening... let go to stop", W / 2, 140, F_BODY, COL_CHAMP, D_MC);
      int lvl = min(1.0f, recPeak / 20000.0f) * (W - 80);
      rfill(40, 162, W - 80, 10, 5, COL_LINE);
      rfill(40, 162, max(10, lvl), 10, 5, COL_GOLD);
      recPeak = recPeak * 6 / 10;
      char s[24];
      snprintf(s, sizeof s, "%.1f s of %d", (now - t0) / 1000.0f, MAX_REC_SEC);
      txt(s, W / 2, 190, F_SMALL, COL_MIST, D_MC);
      pushNow();
    }
  }
  needDraw = true;
  if (recLen < AUDIO_RATE / 2) { showNote("Hold the button while you talk.", COL_GOLD); return; }
  recNormalize();
  if (!wifiOn || !sttUrl[0]) {
    showNote("Voice to text needs Wi-Fi and a speech server: Settings > Web settings.", COL_ROSE);
    return;
  }
  sttWant = true;
  uint32_t w0 = millis();
  while (!wifiUp() && millis() - w0 < 20000) {
    busyScreen("Joining Wi-Fi...");
    wifiLoop();
    netLoop();
    delay(50);
    if (wState == W_OFF && wErr[0]) break;
  }
  if (!wifiUp()) { sttWant = false; showNote(wErr[0] ? wErr : "Couldn't join Wi-Fi.", COL_ROSE); return; }
  busyScreen("Turning your words into text...");
  String err;
  String t = transcribe(recBuf, recLen, err);
  sttWant = false;
  if (!t.length()) { showNote(err.c_str(), COL_ROSE); return; }
  int cap = thKind ? MAX_CHAN_TEXT : MAX_TEXT;
  strlcpy(voiceText, t.c_str(), min((int)sizeof voiceText, cap + 1));
  scr = SCR_VOICE;
}

static void drawVoice() {
  fillR(0, 0, W, H, COL_LAC);
  titleBar((String("To ") + peerTitle(thKind, thPeer)).c_str(), false, nullptr);
  paragraph(voiceText, 14, 44, W - 28, F_BODY, COL_IVORY, 20, 7);
  const char *labels[4] = {"Send", "Edit", "Again", "Cancel"};
  dock(labels, 4, 0);
}

// ---------- settings ----------
enum : uint8_t { S_NAME, S_WIFI, S_WEB, S_HOME, S_TZ, S_H24, S_VIS, S_RELAY, S_SOUND, S_READ, S_VOL, S_SLEEP, S_ROT, S_ABOUT, S_ERASE, S_COUNT };

static String settingValue(uint8_t s) {
  switch (s) {
    case S_NAME: return String(myNick);
    case S_WIFI: return !wifiOn ? String("Off") : String(wSsid) + (wState == W_UP ? "" : wErr[0] ? " (not joined)" : "");
    case S_WEB: return wifiUp() ? String("http://") + WiFi.localIP().toString() : String("Needs Wi-Fi");
    case S_HOME: return homeCam >= 0 && homeCam < nCams ? String(cams[homeCam].name) : String("Logo and clock");
    case S_TZ: return String(TZS[tzIdx].name);
    case S_H24: return h24 ? "On" : "Off";
    case S_VIS: return discoverable ? "On" : "Off";
    case S_RELAY: return relayOn ? "On" : "Off";
    case S_SOUND: return soundOn ? "On" : "Off";
    case S_READ: return autoRead ? "On" : "Off";
    case S_VOL: return VOL_LBL[volIdx];
    case S_SLEEP: return SLEEP_LBL[sleepIdx];
    case S_ROT: return flip180 ? "Flipped" : "Normal";
    case S_ABOUT: return String(myId + PREFIX_LEN) + "  v" FW_VERSION + (LONG_RANGE ? "  Long Range" : "  Standard");
    default: return "";
  }
}
static const char *SETTING_LBL[S_COUNT] = {"Your name", "Wi-Fi", "Web settings", "Home screen", "Time zone", "24-hour clock",
  "Show me in Nearby", "Repeat for others", "Sounds", "Read new messages aloud", "Volume", "Screen sleep", "Rotate screen",
  "Device ID", "Erase everything"};

static void drawSettings() {
  fillR(0, 0, W, H, COL_LAC);
  int y0 = BAR_H - scrollY;
  for (int i = 0; i < S_COUNT; i++) {
    int y = y0 + i * 36;
    if (y + 36 <= BAR_H || y >= H) continue;
    txt(SETTING_LBL[i], 12, y + 18, F_BODY, i == S_ERASE ? COL_ROSE : COL_IVORY, D_ML);
    String v = settingValue(i);
    txt(fitText(v.c_str(), 130, F_SMALL).c_str(), W - 12, y + 18, F_SMALL, COL_GOLD, D_MR);
    hline(12, y + 35, W - 24, COL_LINE);
  }
  scrollMax = max(0, S_COUNT * 36 - (H - BAR_H));
  titleBar("Settings", true, nullptr);
}

static void settingTap(uint8_t s) {
  switch (s) {
    case S_NAME: openEditor(ED_MYNAME, "Your name", myNick, 23, false, "Shown to people you message."); return;
    case S_WIFI: scr = SCR_WIFI; scrollY = 0; startWifiScan(); break;
    case S_WEB: {
      if (!wifiOn) { showNote("Join Wi-Fi first.", COL_ROSE); break; }
      webWant = true;   // keeps Wi-Fi up while you're on the settings screens
      char m[80];
      snprintf(m, sizeof m, "User ca53y, code %s at %s", webCode, wifiUp() ? WiFi.localIP().toString().c_str() : "(joining)");
      showNote(m, COL_GOLD);
      toast.until = millis() + 12000;
      break;
    }
    case S_HOME: openPick(PK_HOME); return;
    case S_TZ: openPick(PK_TZ); return;
    case S_H24: h24 = !h24; prefs.putBool("h24", h24); break;
    case S_VIS: discoverable = !discoverable; prefs.putBool("disc", discoverable); break;
    case S_RELAY: relayOn = !relayOn; prefs.putBool("relay", relayOn); break;
    case S_SOUND: soundOn = !soundOn; prefs.putBool("snd", soundOn); break;
    case S_READ: autoRead = !autoRead; prefs.putBool("read", autoRead); if (autoRead) say("New messages will be read aloud."); break;
    case S_VOL: openPick(PK_VOL); return;
    case S_SLEEP: openPick(PK_SLEEP); return;
    case S_ROT: flip180 = !flip180; prefs.putBool("flip", flip180); lcd.setRotation(flip180 ? 3 : 1); break;
    case S_ABOUT: showNote(myId, COL_GOLD); break;
    case S_ERASE:
      confirmAction = CONF_ERASE;
      scr = SCR_CONFIRM;
      break;
  }
  needDraw = true;
}

// ---------- Wi-Fi ----------
static void startWifiScan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);
  scanCount = -1;
}

static void drawWifi() {
  fillR(0, 0, W, H, COL_LAC);
  int y0 = BAR_H - scrollY, row = 0;
  auto r = [&](const char *a, const char *b, uint32_t ca) {
    int y = y0 + row * 38;
    if (y + 38 > BAR_H && y < H) {
      txt(fitText(a, 200, F_BODY).c_str(), 12, y + 19, F_BODY, ca, D_ML);
      if (b) txt(b, W - 12, y + 19, F_SMALL, COL_MIST, D_MR);
      hline(12, y + 37, W - 24, COL_LINE);
    }
    row++;
  };
  if (wifiOn) r((String("Forget ") + wSsid).c_str(), wState == W_UP ? "joined" : wErr[0] ? wErr : "saved", COL_ROSE);
  if (scanCount < 0) r("Looking for networks...", nullptr, COL_MIST);
  for (int i = 0; i < scanCount; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    char b[16];
    snprintf(b, sizeof b, "%s ch %d", WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "", (int)WiFi.channel(i));
    r(s.c_str(), b, WiFi.channel(i) == ESPNOW_CHANNEL ? COL_VERD : COL_IVORY);
  }
  r("Other network...", nullptr, COL_GOLD);
  scrollMax = max(0, row * 38 - (H - BAR_H - 20));
  fillR(0, H - 20, W, 20, COL_LAC2);
  txt("Green = channel 1: the mesh keeps running while joined", W / 2, H - 10, F_SMALL, COL_MIST, D_MC);
  titleBar("Wi-Fi", true, "Rescan");
}

static void wifiTap(int x, int y) {
  if (y < BAR_H) { if (x > W - 80) startWifiScan(); else scr = SCR_SETTINGS; needDraw = true; return; }
  int row = (y - BAR_H + scrollY) / 38;
  if (wifiOn) { if (row == 0) { wifiForget(); needDraw = true; return; } row--; }
  if (scanCount < 0) { if (row == 0) return; row--; }
  int k = 0;
  for (int i = 0; i < max(0, scanCount); i++) {
    if (!WiFi.SSID(i).length()) continue;
    if (k++ == row) {
      strlcpy(wPickSsid, WiFi.SSID(i).c_str(), sizeof wPickSsid);
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) { wifiSet(wPickSsid, ""); scr = SCR_SETTINGS; showNote("Joining Wi-Fi...", COL_GOLD); }
      else { openEditor(ED_W_PASS, (String("Password for ") + wPickSsid).c_str(), "", 63, true, "Saved on this device only."); ed.ret = SCR_WIFI; }
      return;
    }
  }
  if (row == k) { openEditor(ED_W_SSID, "Network name", "", 32, false, "Exactly as it appears on your router."); ed.ret = SCR_WIFI; }
}

// ---------- cameras ----------
int viewCam = 0;

static void drawCams() {
  fillR(0, 0, W, H, COL_LAC);
  for (int i = 0; i < nCams; i++) {
    int y = BAR_H - scrollY + i * ROW_H;
    listRow(y, cams[i].name, String(cams[i].url), homeCam == i ? COL_GOLD : COL_IVORY, 0, false, 0);
  }
  if (!nCams) paragraph(wifiOn ? "No cameras yet. Tap + to add one, or add them from Web settings on your phone."
                               : "Join Wi-Fi in Settings, then tap + to add a camera.", 20, 70, W - 40, F_BODY, COL_MIST, 20);
  scrollMax = max(0, nCams * ROW_H - (H - BAR_H));
  titleBar("Cameras", true, "+");
}

static void drawCamView() {
  drawCamFrame();
  if (camOverlay) {
    G->fillRect(0, 0, W, 26, COL_BLACK);
    txt(fitText(cams[viewCam].name, 200, F_BODY).c_str(), 10, 13, F_BODY, COL_CHAMP, D_ML);
    if (meshPaused()) txt("Mesh paused", W - 10, 13, F_SMALL, COL_ROSE, D_MR);
    const char *labels[4] = {"Back", "Next", homeCam == viewCam ? "Home: on" : "Home", "Remove"};
    dock(labels, 4, -1);
  }
}

// ---------- pick lists ----------
static int pickCount() {
  switch (pickKind) {
    case PK_ADD: return 2;
    case PK_TZ: return N_TZ;
    case PK_HOME: return 1 + nCams;
    case PK_VOL: return 4;
    case PK_SLEEP: return 4;
  }
  return 0;
}
static String pickLabel(int i) {
  switch (pickKind) {
    case PK_ADD: return i == 0 ? "Add a contact by ID" : "Join a channel";
    case PK_TZ: return TZS[i].name;
    case PK_HOME: return i == 0 ? String("CA53Y logo and clock") : String(cams[i - 1].name);
    case PK_VOL: return VOL_LBL[i];
    case PK_SLEEP: return SLEEP_LBL[i];
  }
  return "";
}
static int pickCurrent() {
  switch (pickKind) {
    case PK_TZ: return tzIdx;
    case PK_HOME: return homeCam + 1;
    case PK_VOL: return volIdx;
    case PK_SLEEP: return sleepIdx;
  }
  return -1;
}
static void openPick(uint8_t kind) { pickKind = kind; scrollY = 0; scr = SCR_PICK; needDraw = true; }

static void drawPick() {
  fillR(0, 0, W, H, COL_LAC);
  int n = pickCount(), cur = pickCurrent();
  for (int i = 0; i < n; i++) {
    int y = BAR_H - scrollY + i * 36;
    if (y + 36 <= BAR_H || y >= H) continue;
    if (i == cur) circ(18, y + 18, 5, COL_GOLD); else ring(18, y + 18, 5, COL_LINE);
    txt(pickLabel(i).c_str(), 34, y + 18, F_BODY, COL_IVORY, D_ML);
    hline(12, y + 35, W - 24, COL_LINE);
  }
  scrollMax = max(0, n * 36 - (H - BAR_H));
  const char *t[] = {"Add", "Time zone", "Home screen", "Volume", "Screen sleep"};
  titleBar(t[pickKind], true, nullptr);
}

static void openContactForm(int ci, const char *prefId, const char *prefNick);
static void openChanForm(int chi);

static void pickTap(int i) {
  uint8_t back = SCR_SETTINGS;
  switch (pickKind) {
    case PK_ADD:
      if (i == 0) openContactForm(-1, "", "");
      else openChanForm(-1);
      return;
    case PK_TZ: tzIdx = i; prefs.putUChar("tz", tzIdx); applyTz(); break;
    case PK_HOME: homeCam = i - 1; prefs.putChar("home", homeCam); break;
    case PK_VOL: setVolume(i); prefs.putUChar("vol", volIdx); chime(); break;
    case PK_SLEEP: sleepIdx = i; prefs.putUChar("slp", sleepIdx); break;
  }
  scr = back;
  needDraw = true;
}

// ---------- contact / channel forms ----------
static void formRow(int y, const char *label, const char *value, bool enabled) {
  txt(label, 12, y + 18, F_BODY, enabled ? COL_IVORY : COL_MIST, D_ML);
  txt(fitText(value, 170, F_SMALL).c_str(), W - 12, y + 18, F_SMALL, enabled ? COL_GOLD : COL_MIST, D_MR);
  hline(12, y + 37, W - 24, COL_LINE);
}

static void openContactForm(int ci, const char *prefId, const char *prefNick) {
  memset(&cf, 0, sizeof cf);
  cf.editing = ci >= 0;
  if (ci >= 0) {
    strlcpy(cf.id, contacts[ci].id, sizeof cf.id);
    strlcpy(cf.nick, contacts[ci].nick, sizeof cf.nick);
    cf.hadPin = contacts[ci].pin[0];
  } else {
    strlcpy(cf.id, prefId, sizeof cf.id);
    strlcpy(cf.nick, prefNick, sizeof cf.nick);
  }
  scr = SCR_CONTACT;
  needDraw = true;
}

static void drawContact() {
  fillR(0, 0, W, H, COL_LAC);
  titleBar(cf.editing ? "Contact" : "New contact", true, nullptr);
  formRow(BAR_H + 4, "Device ID", cf.id[0] ? cf.id : "CA53Ycomms-...", !cf.editing);
  formRow(BAR_H + 42, "Name", cf.nick[0] ? cf.nick : "(optional)", true);
  const char *pin = cf.pinMode == 1 ? "Will be set" : cf.pinMode == 2 ? "Will be removed" : cf.hadPin ? "Set" : "Not set";
  formRow(BAR_H + 80, "Shared PIN", pin, true);
  paragraph("With a shared PIN, messages are encrypted. They must set the same PIN for you.", 12, BAR_H + 124, W - 24, F_SMALL, COL_MIST, 16, 3);
  const char *a[3] = {"Save", "Delete", "Cancel"};
  const char *b[2] = {"Save", "Cancel"};
  if (cf.editing) dock(a, 3, 0); else dock(b, 2, 0);
}

static void saveContactForm() {
  char id[20];
  if (!normalizeId(String(cf.id), id)) { showNote("IDs look like CA53Ycomms-1234abC", COL_ROSE); return; }
  if (!strcasecmp(id, myId)) { showNote("That's this device's own ID.", COL_ROSE); return; }
  int ci = findContact(id);
  if (ci < 0) ci = addContact(id, cf.nick);
  if (ci < 0) { showNote("Contact list is full (16).", COL_ROSE); return; }
  strlcpy(contacts[ci].nick, cf.nick, sizeof contacts[ci].nick);
  if (cf.pinMode == 1) strlcpy(contacts[ci].pin, cf.pin, sizeof contacts[ci].pin);
  if (cf.pinMode == 2) contacts[ci].pin[0] = 0;
  crt[ci].isNew = false;
  deriveKey(ci);
  saveContacts();
  openThread(0, contacts[ci].id);
}

static void contactTap(int x, int y) {
  if (y < BAR_H) { scr = cf.editing ? SCR_THREAD : SCR_LIST; needDraw = true; return; }
  int d = dockHit(x, y, cf.editing ? 3 : 2);
  if (d == 0) { saveContactForm(); return; }
  if (cf.editing && d == 1) { confirmAction = CONF_DEL_CONTACT; confirmIdx = findContact(cf.id); scr = SCR_CONFIRM; needDraw = true; return; }
  if (d >= 0) { scr = cf.editing ? SCR_THREAD : SCR_LIST; needDraw = true; return; }
  if (y < BAR_H + 42 && !cf.editing) openEditor(ED_C_ID, "Device ID", cf.id[0] ? cf.id + (strlen(cf.id) > 11 ? 11 : 0) : "", 18, false, "The 7 characters after CA53Ycomms-");
  else if (y >= BAR_H + 42 && y < BAR_H + 80) openEditor(ED_C_NICK, "Name", cf.nick, 23, false, "What you call them");
  else if (y >= BAR_H + 80 && y < BAR_H + 118) openEditor(ED_C_PIN, "Shared PIN", "", 32, true, "4 to 32 characters. Leave empty to remove it.");
}

static void openChanForm(int chi) {
  memset(&chf, 0, sizeof chf);
  chf.idx = chi;
  if (chi >= 0) { strlcpy(chf.name, chans[chi].name, sizeof chf.name); strlcpy(chf.pass, chans[chi].pass, sizeof chf.pass); }
  scr = SCR_CHAN;
  needDraw = true;
}

static void drawChan() {
  fillR(0, 0, W, H, COL_LAC);
  titleBar(chf.idx >= 0 ? "Channel" : "Join a channel", true, nullptr);
  formRow(BAR_H + 4, "Name", chf.name[0] ? chf.name : "(required)", chf.idx < 0);
  formRow(BAR_H + 42, "Passphrase", chf.pass[0] ? "Set" : "None (open)", chf.idx < 0);
  paragraph("Everyone who joins the same name and passphrase is in the same group. No passphrase means anyone can read it.", 12, BAR_H + 86, W - 24, F_SMALL, COL_MIST, 16, 4);
  const char *a[2] = {"Leave", "Back"};
  const char *b[2] = {"Join", "Cancel"};
  dock(chf.idx >= 0 ? a : b, 2, chf.idx >= 0 ? -1 : 0);
}

static void chanTap(int x, int y) {
  if (y < BAR_H) { scr = SCR_LIST; needDraw = true; return; }
  int d = dockHit(x, y, 2);
  if (chf.idx >= 0) {
    if (d == 0) { confirmAction = CONF_LEAVE_CHAN; confirmIdx = chf.idx; scr = SCR_CONFIRM; needDraw = true; }
    else if (d == 1) { scr = SCR_THREAD; needDraw = true; }
    return;
  }
  if (d == 0) {
    if (!chf.name[0]) { showNote("Give the channel a name.", COL_ROSE); return; }
    int chi = findChanByName(chf.name, chf.pass);
    if (chi < 0) chi = addChannel(chf.name, chf.pass);
    if (chi < 0) { showNote("Channel list is full (8).", COL_ROSE); return; }
    saveChannels();
    openThread(1, chrt[chi].addr);
    return;
  }
  if (d == 1) { scr = SCR_LIST; needDraw = true; return; }
  if (y < BAR_H + 42) openEditor(ED_CH_NAME, "Channel name", chf.name, 23, false, "Same name for everyone in the group");
  else if (y < BAR_H + 80) openEditor(ED_CH_PASS, "Passphrase", chf.pass, 32, true, "Leave empty for an open channel");
}

// ---------- confirm ----------
static void drawConfirm() {
  fillR(0, 0, W, H, COL_LAC);
  const char *t = "", *b = "";
  switch (confirmAction) {
    case CONF_ERASE: t = "Erase everything?"; b = "Removes your name, contacts, channels, Wi-Fi and cameras. The device ID stays the same."; break;
    case CONF_DEL_CONTACT: t = "Delete contact?"; b = "Removes them and their PIN from this device."; break;
    case CONF_LEAVE_CHAN: t = "Leave channel?"; b = "You can rejoin later with the same name and passphrase."; break;
    case CONF_DEL_CAM: t = "Remove camera?"; b = "Removes it from this device."; break;
  }
  txt(t, W / 2, 50, F_TITLE, COL_CHAMP, D_MC);
  decoRule(72);
  paragraph(b, 24, 90, W - 48, F_BODY, COL_IVORY, 20, 4);
  const char *labels[2] = {"Yes", "Cancel"};
  dock(labels, 2, 0);
}

static void confirmTap(int x, int y) {
  int d = dockHit(x, y, 2);
  if (d < 0) return;
  uint8_t a = confirmAction;
  needDraw = true;
  if (d == 1) { scr = a == CONF_ERASE ? SCR_SETTINGS : a == CONF_DEL_CAM ? SCR_CAMVIEW : a == CONF_DEL_CONTACT ? SCR_CONTACT : SCR_CHAN; return; }
  switch (a) {
    case CONF_ERASE: {
      String id = prefs.getString("id", "");
      String code = prefs.getString("webc", "");
      prefs.clear();
      prefs.putString("id", id);
      prefs.putString("webc", code);
      ESP.restart();
      break;
    }
    case CONF_DEL_CONTACT: deleteContact(confirmIdx); scr = SCR_LIST; break;
    case CONF_LEAVE_CHAN: deleteChannel(confirmIdx); scr = SCR_LIST; break;
    case CONF_DEL_CAM:
      for (int i = viewCam; i < nCams - 1; i++) cams[i] = cams[i + 1];
      nCams--;
      if (homeCam == viewCam) homeCam = -1; else if (homeCam > viewCam) homeCam--;
      prefs.putChar("home", homeCam);
      saveCams();
      camStop();
      scr = SCR_CAMS;
      break;
  }
}

// ---------- render ----------
static void drawToast() {
  if (!toast.on) return;
  if ((int32_t)(millis() - toast.until) > 0) { toast.on = false; return; }
  G->setFont(F_SMALL);
  char a[100];
  asciiize(a, toast.text, sizeof a, true);
  uint16_t ls[4]; uint8_t ll[4];
  int n = wrapText(a, W - 40, F_SMALL, ls, ll, 3);
  int h = n * 16 + 12, y = (scr == SCR_KEYS ? 70 : DOCK_Y) - h - 4;
  rfill(10, y, W - 20, h, 4, COL_INK);
  rframe(10, y, W - 20, h, 4, toast.color);
  drawLines(a, ls, ll, n, 20, y + 6, 16, F_SMALL, toast.color);
}

static void render() {
  G = &spr;
  switch (scr) {
    case SCR_HOME: drawHome(); break;
    case SCR_ALERT: drawAlert(); break;
    case SCR_LIST: drawList(); break;
    case SCR_THREAD: drawThread(); break;
    case SCR_KEYS: drawKeys(); break;
    case SCR_VOICE: drawVoice(); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_WIFI: drawWifi(); break;
    case SCR_CAMS: drawCams(); break;
    case SCR_CAMVIEW: drawCamView(); break;
    case SCR_PICK: drawPick(); break;
    case SCR_CONTACT: drawContact(); break;
    case SCR_CHAN: drawChan(); break;
    case SCR_CONFIRM: drawConfirm(); break;
  }
  drawToast();
  spr.pushSprite(0, 0);
  if (scr == SCR_CAMVIEW || (scr == SCR_HOME && homeCam >= 0)) camReady = false;
}

// ---------- incoming ----------
bool pendingRead = false;

void wake() {
  lastInput = millis();
  if (asleep) { asleep = false; backlight(true); needDraw = true; }
}

void onIncoming(Msg *m) {
  wake();
  bool here = scr == SCR_THREAD && !strcasecmp(thPeer, m->peer);
  if (here) {
    if (m->kind == 0) { int ci = findContact(m->peer); if (ci >= 0) crt[ci].unread = 0; }
    else { int chi = findChanByAddr(m->peer); if (chi >= 0) chrt[chi].unread = 0; }
  } else if (scr == SCR_HOME || scr == SCR_ALERT || scr == SCR_LIST || scr == SCR_CAMVIEW || scr == SCR_CAMS) {
    alertUid = m->uid;
    scr = SCR_ALERT;
  } else {
    char t[80];
    snprintf(t, sizeof t, "%s: %s", senderName(m).c_str(), m->st == 11 ? "(locked)" : m->text);
    showNote(t, COL_GOLD);
  }
  if (soundOn) pendingRead = true;   // chime (and read aloud if that's on) after the screen updates
  needDraw = true;
}

static void readAloud(Msg *m) {
  if (!m || m->st == 11) return;
  String s = senderName(m) + " says. " + m->text;
  say(s.c_str());
}

// ---------- taps ----------
static void homeTap(int x, int y) {
  int d = dockHit(x, y, 3);
  if (d == 0) { scr = SCR_LIST; listTab = 0; scrollY = 0; }
  else if (d == 1) { scr = SCR_CAMS; scrollY = 0; }
  else if (d == 2) { scr = SCR_SETTINGS; scrollY = 0; }
  else if (y > 30) { scr = SCR_LIST; listTab = 0; scrollY = 0; }
  needDraw = true;
}

static void alertTap(int x, int y) {
  Msg *m = findMsg(alertUid);
  int d = dockHit(x, y, 4);
  if (!m) { scr = SCR_HOME; needDraw = true; return; }
  if (d == 0) { readAloud(m); return; }
  if (d == 1 || d == 2) { openThread(m->kind, m->peer); return; }
  if (d == 3) { scr = SCR_HOME; needDraw = true; }
}

static void listTap(int x, int y) {
  if (y < BAR_H) {
    if (x < 70) scr = SCR_HOME;
    else if (x > W - 50) openPick(PK_ADD);
    needDraw = true;
    return;
  }
  int d = dockHit(x, y, 2);
  if (d >= 0) { listTab = d; scrollY = 0; needDraw = true; return; }
  int i = (y - BAR_H + scrollY) / ROW_H;
  if (listTab == 0) {
    Row rows[MAX_CONTACTS + MAX_CHANNELS];
    int n = buildChatRows(rows);
    if (i < 0 || i >= n) return;
    if (rows[i].kind == 0) openThread(0, contacts[rows[i].idx].id);
    else openThread(1, chrt[rows[i].idx].addr);
  } else {
    Nearby *nb[MAX_NEARBY];
    int n = nearbyList(nb);
    if (i < 0 || i >= n) return;
    int ci = findContact(nb[i]->id);
    if (ci >= 0) openThread(0, contacts[ci].id);
    else openContactForm(-1, nb[i]->id, nb[i]->nick);
  }
}

static void threadTap(int x, int y) {
  if (y < BAR_H) {
    if (x < 90) { scr = SCR_LIST; needDraw = true; }
    else if (x > W - 60) {
      if (thKind == 0) openContactForm(findContact(thPeer), "", "");
      else openChanForm(findChanByAddr(thPeer));
    }
    return;
  }
  if (dockHit(x, y, 2) == 0) {
    openEditor(ED_MSG, (String("To ") + peerTitle(thKind, thPeer)).c_str(), "", thKind ? MAX_CHAN_TEXT : MAX_TEXT, false, "Type your message");
    return;
  }
  if (y >= TH_BOT) return;
  int cy = y - BAR_H + scrollY;   // tap a message: read it aloud, or resend a failed one
  for (int i = 0; i < tcount; i++) {
    if (cy < titems[i].y || cy > titems[i].y + titems[i].h) continue;
    Msg *m = findMsg(titems[i].uid);
    if (!m) return;
    if (m->dir == 1 && m->st == 2) { resendFailed(m->uid); needDraw = true; }
    else if (m->dir == 0) readAloud(m);
    return;
  }
}

static void voiceTap(int x, int y) {
  int d = dockHit(x, y, 4);
  if (d == 0) { sendText(voiceText); scr = SCR_THREAD; }
  else if (d == 1) { openEditor(ED_VOICE, "Edit before sending", voiceText, thKind ? MAX_CHAN_TEXT : MAX_TEXT, false, ""); ed.ret = SCR_THREAD; return; }
  else if (d == 2) { scr = SCR_THREAD; showNote("Hold the button and speak again.", COL_GOLD); }
  else if (d == 3) scr = SCR_THREAD;
  needDraw = true;
}

static void camsTap(int x, int y) {
  if (y < BAR_H) {
    if (x < 90) scr = SCR_HOME;
    else if (x > W - 50) {
      if (!wifiOn) { showNote("Join Wi-Fi in Settings first.", COL_ROSE); return; }
      if (nCams >= MAX_CAMS) { showNote("Camera list is full.", COL_ROSE); return; }
      openEditor(ED_CAM_NAME, "Camera name", "", 23, false, "Front door, garage...");
      return;
    }
    needDraw = true;
    return;
  }
  int i = (y - BAR_H + scrollY) / ROW_H;
  if (i >= 0 && i < nCams) { viewCam = i; camOverlay = true; camOverlayAt = millis(); scr = SCR_CAMVIEW; needDraw = true; }
}

static void camViewTap(int x, int y) {
  if (!camOverlay) { camOverlay = true; camOverlayAt = millis(); needDraw = true; return; }
  camOverlayAt = millis();
  int d = dockHit(x, y, 4);
  if (d == 0) scr = SCR_CAMS;
  else if (d == 1 && nCams > 1) { viewCam = (viewCam + 1) % nCams; }
  else if (d == 2) { homeCam = homeCam == viewCam ? -1 : viewCam; prefs.putChar("home", homeCam); showNote(homeCam >= 0 ? "Shown on the home screen" : "Home screen shows the clock", COL_GOLD); }
  else if (d == 3) { confirmAction = CONF_DEL_CAM; scr = SCR_CONFIRM; }
  else camOverlay = false;
  needDraw = true;
}

static void onTap(int x, int y) {
  lastInput = millis();
  switch (scr) {
    case SCR_HOME: homeTap(x, y); break;
    case SCR_ALERT: alertTap(x, y); break;
    case SCR_LIST: listTap(x, y); break;
    case SCR_THREAD: threadTap(x, y); break;
    case SCR_KEYS: keyTap(x, y); break;
    case SCR_VOICE: voiceTap(x, y); break;
    case SCR_SETTINGS:
      if (y < BAR_H) { scr = SCR_HOME; webWant = false; needDraw = true; }
      else settingTap((y - BAR_H + scrollY) / 36);
      break;
    case SCR_WIFI: wifiTap(x, y); break;
    case SCR_CAMS: camsTap(x, y); break;
    case SCR_CAMVIEW: camViewTap(x, y); break;
    case SCR_PICK:
      if (y < BAR_H) { scr = pickKind == PK_ADD ? SCR_LIST : SCR_SETTINGS; needDraw = true; }
      else { int i = (y - BAR_H + scrollY) / 36; if (i >= 0 && i < pickCount()) pickTap(i); }
      break;
    case SCR_CONTACT: contactTap(x, y); break;
    case SCR_CHAN: chanTap(x, y); break;
    case SCR_CONFIRM: confirmTap(x, y); break;
  }
}

static bool scrollable() {
  return scr == SCR_LIST || scr == SCR_THREAD || scr == SCR_SETTINGS || scr == SCR_WIFI || scr == SCR_CAMS || scr == SCR_PICK;
}

// ---------- touch ----------
void touchLoop() {
  static bool down = false, dragging = false, swallow = false;
  static int sx, sy, ly;
  static uint32_t lastPoll = 0;
  uint32_t now = millis();
  if (now - lastPoll < 15) return;
  lastPoll = now;
  int x, y;
  bool p = touchRead(x, y);
  if (p && !down) {
    down = true; dragging = false; swallow = false;
    sx = x; sy = y; ly = y;
    if (asleep) { wake(); swallow = true; return; }
    lastInput = now;
    if (scr == SCR_THREAD && dockHit(x, y, 2) == 1) { runTalk(); swallow = true; needDraw = true; return; }
    return;
  }
  if (p && down) {
    lastInput = now;
    if (swallow) return;
    if (!dragging && abs(y - sy) > 10 && scrollable()) dragging = true;
    if (dragging) {
      scrollY = constrain(scrollY - (y - ly), 0, scrollMax);
      if (scr == SCR_THREAD) thStick = scrollY >= scrollMax;
      ly = y;
      needDraw = true;
    }
    return;
  }
  if (!p && down) {
    down = false;
    if (!swallow && !dragging) onTap(sx, sy);
  }
}

// ---------- loop pieces ----------
void cameraPolicy() {
  int want = -1;
  if (scr == SCR_CAMVIEW && viewCam < nCams) want = viewCam;
  else if (scr == SCR_HOME && homeCam >= 0 && homeCam < nCams && !asleep) want = homeCam;
  camWant = want >= 0;
  if (want != camIdx) { if (want < 0) camStop(); else camStart(want); needDraw = true; }
  if (camReady && (scr == SCR_CAMVIEW || scr == SCR_HOME)) needDraw = true;
  if (scr == SCR_CAMVIEW && camOverlay && millis() - camOverlayAt > 5000) { camOverlay = false; needDraw = true; }
}

void powerLoop() {
  uint32_t ms = SLEEP_MS[sleepIdx];
  bool keepOn = scr == SCR_HOME && homeCam >= 0;   // a camera on the home screen is meant to stay visible
  if (!asleep && ms && !keepOn && millis() - lastInput > ms) {
    asleep = true;
    backlight(false);
    if (scr != SCR_HOME) { scr = SCR_HOME; needDraw = true; }
  }
}

void bootButtonLoop() {
  static uint32_t pressStart = 0;
  if (digitalRead(PIN_BOOT) == LOW) {
    uint32_t now = millis();
    if (!pressStart) pressStart = now;
    if (now - pressStart > 5000) {
      String id = prefs.getString("id", "");
      prefs.clear();
      prefs.putString("id", id);
      ESP.restart();
    }
  } else if (pressStart) {
    if (millis() - pressStart < 1500) {   // short press: wake / go home
      if (asleep) wake();
      else { scr = SCR_HOME; needDraw = true; lastInput = millis(); }
    }
    pressStart = 0;
  }
}

// Logs changes on the expander's input pins, so the board's other buttons can be mapped later.
void expanderWatch() {
  static uint32_t at = 0;
  static uint16_t last = 0xFFFF;
  static bool first = true;
  if (millis() - at < 60) return;
  at = millis();
  uint16_t v = xlInputs();
  if (first) { last = v; first = false; return; }
  if (v != last) { Serial.printf("[xl9555] inputs %04X -> %04X\n", last, v); last = v; }
}

void uiLoop() {
  touchLoop();
  cameraPolicy();
  powerLoop();
  bootButtonLoop();
  expanderWatch();

  static uint32_t lastDraw = 0, lastMin = 0;
  uint32_t now = millis();
  if (scr == SCR_WIFI && scanCount < 0) {
    int c = WiFi.scanComplete();
    if (c >= 0 || c == -2) {
      scanCount = max(0, c);
      if (wState == W_OFF) esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
      needDraw = true;
    }
  }
  if (webWant && scr != SCR_SETTINGS && scr != SCR_WIFI && scr != SCR_PICK) webWant = false;
  if (netChanged) { netChanged = false; if (scr != SCR_KEYS) needDraw = true; }
  if (time(nullptr) / 60 != lastMin) { lastMin = time(nullptr) / 60; if (scr == SCR_HOME) needDraw = true; }
  if (scr == SCR_KEYS && now - lastDraw > 500) needDraw = true;   // cursor blink
  if (toast.on && (int32_t)(now - toast.until) > 0) needDraw = true;
  if (needDraw && !asleep && now - lastDraw > 30) {
    needDraw = false;
    lastDraw = now;
    render();
  }
  if (pendingRead) {
    pendingRead = false;
    chime();
    if (autoRead && scr == SCR_ALERT) readAloud(findMsg(alertUid));
  }
}

void uiSetup() {
  lcd.init();
  flip180 = prefs.getBool("flip", false);
  autoRead = prefs.getBool("read", false);
  setVolume(prefs.getUChar("vol", 2));
  lcd.setRotation(flip180 ? 3 : 1);
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) Serial.println("Sprite allocation failed (is PSRAM enabled?)");
  initCharWidths();
  G = &spr;
  fillR(0, 0, W, H, COL_LAC);
  emblem(W / 2, 100, 1.0f, COL_LAC);
  txt("Comms", W / 2, 200, F_TITLE, COL_CHAMP, D_MC);
  txt(myId, W / 2, 224, F_SMALL, COL_MIST, D_MC);
  spr.pushSprite(0, 0);
  lastInput = millis();
}

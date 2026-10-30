// CA53Y Comms wire protocol v2: radio, mesh relay, encryption, contacts, channels, messages.
// Over-the-air compatible with the CA53Y Comms dev-module messengers and the relay boards.
// See PROTOCOL.md in the repo for the byte layout.
#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_random.h>
#include <Preferences.h>
#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "Needs Arduino-ESP32 core 3.x: Tools > Board > Boards Manager > esp32 by Espressif > 3.x"
#endif

// ---------- wire format ----------
static const char ID_PREFIX[] = "CA53Ycomms-";   // device IDs: CA53Ycomms-1234abC
#define PREFIX_LEN 11
#define ID_LEN     18

enum : uint8_t { T_BEACON = 1, T_MSG = 2, T_ACK = 3 };
#define F_ENC  0x01   // payload is AES-128-GCM
#define F_CHAN 0x02   // message is for a channel; "to" is the channel's address

struct __attribute__((packed)) PktHdr {
  uint8_t m0, m1, ver, type, flags;   // 0xCA 0x53, version 2
  char from[20];
  char to[20];
  uint32_t mid;
  uint8_t ttl;     // hops left; repeaters decrement it (not authenticated)
  uint8_t tries;   // resend counter (not authenticated)
};
#define HDR_LEN   ((int)sizeof(PktHdr))
#define AUTH_LEN  (HDR_LEN - 2)   // ttl + tries change in flight, so GCM leaves them out
#define PROTO_VER 2
#define IV_LEN    12
#define TAG_LEN   16

// ---------- state ----------
struct Contact   { char id[20]; char nick[24]; char pin[33]; };   // saved to flash (same layout as the dev-module messenger)
struct ContactRt { uint8_t key[16]; bool hasKey; uint16_t unread; bool isNew; uint32_t lastAct; };
struct Channel   { char name[24]; char pass[33]; };               // saved to flash
struct ChannelRt { char addr[20]; uint8_t key[16]; bool hasKey; uint16_t unread; uint32_t lastAct; };
struct Nearby    { char id[20]; char nick[24]; int8_t rssi; uint32_t seen; bool used; };
struct Msg {
  uint32_t uid, mid, t;
  uint8_t kind;   // 0 direct, 1 channel
  uint8_t dir;    // 0 in, 1 out
  uint8_t st;     // 0 sending, 1 delivered, 2 failed, 3 delivered but they can't read it, 4 sent (channel),
                  // 10 received, 11 received but locked (PIN mismatch)
  uint8_t enc;
  char peer[20];      // contact ID or channel address
  char from[20];      // sender (channel messages)
  char fromNick[24];  // sender's name as they sent it (channel messages)
  char text[MAX_TEXT + 4];
};
struct Out      { bool used, chan; uint32_t mid, uid, next; uint8_t tries, len; uint8_t buf[250]; };
struct RxItem   { uint8_t len; int8_t rssi; uint8_t data[250]; };
struct Seen     { uint32_t mid; char from[20]; uint8_t ackSt, ackTries; };
struct RelayKey { uint32_t mid; uint8_t type, tries; char from[20]; };
struct RelayOut { bool used; uint32_t at; uint8_t len; uint8_t buf[250]; };

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

Preferences prefs;
QueueHandle_t rxq;

char myId[20], myNick[24];
bool discoverable = true, relayOn = true, soundOn = true, isSetup = false;
uint8_t brightIdx = 2, sleepIdx = 1;

Contact contacts[MAX_CONTACTS];
ContactRt crt[MAX_CONTACTS];
int nContacts = 0;
Channel chans[MAX_CHANNELS];
ChannelRt chrt[MAX_CHANNELS];
int nChans = 0;
Nearby nearby[MAX_NEARBY];
Msg msgs[MAX_MSGS];
int msgHead = 0, msgCount = 0;
Out outq[MAX_OUT];
Seen seen[32];
int seenPos = 0;
Seen chanSeen[48];
int chanSeenPos = 0;
RelayKey relaySeen[64];
int relayPos = 0;
RelayOut relayq[12];
uint32_t gUid = 0, lastBeacon = 0, relayedCount = 0, lastRelayAt = 0, heardCount = 0;
volatile bool netChanged = true;   // tells the UI to redraw

void onIncoming(Msg *m);   // ui.h

// ---------- helpers ----------
bool normalizeId(String s, char *out) {
  s.trim();
  if (s.length() == 7) s = String(ID_PREFIX) + s;
  if (s.length() != ID_LEN) return false;
  if (strncasecmp(s.c_str(), ID_PREFIX, PREFIX_LEN) != 0) return false;
  for (int i = PREFIX_LEN; i < PREFIX_LEN + 4; i++) if (!isdigit((unsigned char)s[i])) return false;
  for (int i = PREFIX_LEN + 4; i < ID_LEN; i++) if (!isalpha((unsigned char)s[i])) return false;
  memcpy(out, ID_PREFIX, PREFIX_LEN);
  memcpy(out + PREFIX_LEN, s.c_str() + PREFIX_LEN, 7);
  out[ID_LEN] = 0;
  return true;
}

// copy text, drop control chars (keep newline if allowed), cut on a UTF-8 boundary
void cleanText(char *dst, size_t cap, const char *src, size_t n, bool allowNewline) {
  if (n > cap - 1) {
    n = cap - 1;
    while (n > 0 && ((uint8_t)src[n] & 0xC0) == 0x80) n--;
  }
  size_t j = 0;
  for (size_t i = 0; i < n && src[i]; i++) {
    char c = src[i];
    if ((uint8_t)c < 0x20) c = (allowNewline && c == '\n') ? '\n' : ' ';
    dst[j++] = c;
  }
  dst[j] = 0;
}

int findContact(const char *id) {
  for (int i = 0; i < nContacts; i++) if (!strcasecmp(contacts[i].id, id)) return i;
  return -1;
}

int findChanByAddr(const char *addr) {
  for (int i = 0; i < nChans; i++) if (!strcasecmp(chrt[i].addr, addr)) return i;
  return -1;
}

int findChanByName(const char *name, const char *pass) {
  for (int i = 0; i < nChans; i++)
    if (!strcasecmp(chans[i].name, name) && !strcmp(chans[i].pass, pass)) return i;
  return -1;
}

Nearby *findNearby(const char *id) {
  for (auto &n : nearby) if (n.used && !strcasecmp(n.id, id)) return &n;
  return nullptr;
}

static void lowerCopy(char *dst, const char *src, size_t cap) {
  size_t i = 0;
  for (; src[i] && i < cap - 1; i++) dst[i] = tolower((unsigned char)src[i]);
  dst[i] = 0;
}

static bool pbkdf2(const char *pass, const String &salt, uint8_t *key) {
  return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
           (const unsigned char *)pass, strlen(pass),
           (const unsigned char *)salt.c_str(), salt.length(),
           KDF_ITERS, 16, key) == 0;
}

// Direct-message key: PBKDF2-SHA256(PIN, "CA53Ycomms|v1|" + both IDs lowercase, sorted). Same as the dev-module messenger.
void deriveKey(int ci) {
  Contact &c = contacts[ci];
  crt[ci].hasKey = false;
  if (!c.pin[0]) return;
  char a[20], b[20];
  lowerCopy(a, myId, sizeof a);
  lowerCopy(b, c.id, sizeof b);
  String salt = "CA53Ycomms|v1|";
  if (strcmp(a, b) < 0) { salt += a; salt += '|'; salt += b; }
  else                  { salt += b; salt += '|'; salt += a; }
  crt[ci].hasKey = pbkdf2(c.pin, salt, crt[ci].key);
}

// Channel address: a device-style ID derived from SHA-256(name + passphrase), so repeaters that only
// understand device IDs still pass channel messages on, and a private channel's name isn't sent in the clear.
// Channel key: PBKDF2-SHA256(passphrase, "CA53Ychan|v1|" + lowercase name). No passphrase = open channel.
void deriveChan(int chi) {
  Channel &c = chans[chi];
  ChannelRt &r = chrt[chi];
  char ln[24];
  lowerCopy(ln, c.name, sizeof ln);
  String s = "CA53Ychan|v1|";
  s += ln; s += '|'; s += c.pass;
  uint8_t h[32];
  mbedtls_sha256((const unsigned char *)s.c_str(), s.length(), h, 0);
  const char *L = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  char suf[8];
  for (int i = 0; i < 4; i++) suf[i] = '0' + h[i] % 10;
  for (int i = 0; i < 3; i++) suf[4 + i] = L[h[4 + i] % 52];
  suf[7] = 0;
  snprintf(r.addr, sizeof r.addr, "%s%s", ID_PREFIX, suf);
  r.hasKey = false;
  if (c.pass[0]) r.hasKey = pbkdf2(c.pass, String("CA53Ychan|v1|") + ln, r.key);
}

void saveContacts() {
  if (nContacts) prefs.putBytes("contacts", contacts, sizeof(Contact) * nContacts);
  else prefs.remove("contacts");
}

void saveChannels() {
  if (nChans) prefs.putBytes("chans", chans, sizeof(Channel) * nChans);
  else prefs.remove("chans");
}

int addContact(const char *id, const char *nick) {
  if (nContacts >= MAX_CONTACTS) return -1;
  int i = nContacts++;
  memset(&contacts[i], 0, sizeof(Contact));
  memset(&crt[i], 0, sizeof(ContactRt));
  strlcpy(contacts[i].id, id, sizeof contacts[i].id);
  if (nick) strlcpy(contacts[i].nick, nick, sizeof contacts[i].nick);
  crt[i].lastAct = millis();
  return i;
}

void deleteContact(int ci) {
  if (ci < 0 || ci >= nContacts) return;
  for (int i = ci; i < nContacts - 1; i++) { contacts[i] = contacts[i + 1]; crt[i] = crt[i + 1]; }
  nContacts--;
  saveContacts();
}

int addChannel(const char *name, const char *pass) {
  if (nChans >= MAX_CHANNELS) return -1;
  int i = nChans++;
  memset(&chans[i], 0, sizeof(Channel));
  memset(&chrt[i], 0, sizeof(ChannelRt));
  strlcpy(chans[i].name, name, sizeof chans[i].name);
  strlcpy(chans[i].pass, pass, sizeof chans[i].pass);
  deriveChan(i);
  chrt[i].lastAct = millis();
  return i;
}

void deleteChannel(int chi) {
  if (chi < 0 || chi >= nChans) return;
  for (int i = chi; i < nChans - 1; i++) { chans[i] = chans[i + 1]; chrt[i] = chrt[i + 1]; }
  nChans--;
  saveChannels();
}

Msg *newMsg() {
  int idx;
  if (msgCount < MAX_MSGS) { idx = (msgHead + msgCount) % MAX_MSGS; msgCount++; }
  else { idx = msgHead; msgHead = (msgHead + 1) % MAX_MSGS; }
  Msg *m = &msgs[idx];
  memset(m, 0, sizeof(Msg));
  m->uid = ++gUid;
  m->t = millis();
  return m;
}

Msg *msgAt(int i) { return &msgs[(msgHead + i) % MAX_MSGS]; }

Msg *findMsg(uint32_t uid) {
  for (int i = 0; i < msgCount; i++) if (msgAt(i)->uid == uid) return msgAt(i);
  return nullptr;
}

void setMsgStatus(uint32_t uid, uint8_t st) {
  Msg *m = findMsg(uid);
  if (m && m->st != st) { m->st = st; netChanged = true; }
}

void fillHdr(uint8_t *buf, uint8_t type, const char *to, uint32_t mid) {
  PktHdr *h = (PktHdr *)buf;
  memset(h, 0, HDR_LEN);
  h->m0 = 0xCA; h->m1 = 0x53; h->ver = PROTO_VER; h->type = type;
  h->ttl = (type == T_BEACON) ? 0 : MAX_HOPS;
  strlcpy(h->from, myId, sizeof h->from);
  if (to) strlcpy(h->to, to, sizeof h->to);
  h->mid = mid;
}

bool gcmEncrypt(const uint8_t *key, const uint8_t *aad, const uint8_t *pt, size_t n,
                uint8_t *iv, uint8_t *tag, uint8_t *ct) {
  mbedtls_gcm_context g;
  mbedtls_gcm_init(&g);
  esp_fill_random(iv, IV_LEN);
  int r = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 128);
  if (!r) r = mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, n, iv, IV_LEN, aad, AUTH_LEN, pt, ct, TAG_LEN, tag);
  mbedtls_gcm_free(&g);
  return r == 0;
}

bool gcmDecrypt(const uint8_t *key, const uint8_t *aad, const uint8_t *iv, const uint8_t *tag,
                const uint8_t *ct, size_t n, uint8_t *pt) {
  mbedtls_gcm_context g;
  mbedtls_gcm_init(&g);
  int r = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 128);
  if (!r) r = mbedtls_gcm_auth_decrypt(&g, n, iv, IV_LEN, aad, AUTH_LEN, tag, TAG_LEN, ct, pt);
  mbedtls_gcm_free(&g);
  return r == 0;
}

void sendBeacon() {
  uint8_t buf[HDR_LEN + 24];
  fillHdr(buf, T_BEACON, nullptr, esp_random());
  memset(buf + HDR_LEN, 0, 24);
  strlcpy((char *)buf + HDR_LEN, myNick, 24);
  esp_now_send(BCAST, buf, sizeof buf);
}

void sendAck(const char *to, uint32_t mid, uint8_t st, uint8_t tries) {
  uint8_t buf[HDR_LEN + 1];
  fillHdr(buf, T_ACK, to, mid);
  ((PktHdr *)buf)->tries = tries;
  buf[HDR_LEN] = st;
  esp_now_send(BCAST, buf, sizeof buf);
}

// ---------- ESP-NOW receive (runs in the Wi-Fi task: just queue it) ----------
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < HDR_LEN || len > 250 || data[0] != 0xCA || data[1] != 0x53) return;
  RxItem it;
  it.len = len;
  it.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  memcpy(it.data, data, len);
  xQueueSend(rxq, &it, 0);
}

// ---------- mesh repeat ----------
bool relayedBefore(const PktHdr &h, const char *from) {
  for (auto &k : relaySeen)
    if (k.mid == h.mid && k.type == h.type && k.tries == h.tries && !strcasecmp(k.from, from)) return true;
  RelayKey &k = relaySeen[relayPos];
  relayPos = (relayPos + 1) % 64;
  k.mid = h.mid; k.type = h.type; k.tries = h.tries;
  strlcpy(k.from, from, sizeof k.from);
  return false;
}

void maybeRelay(const RxItem &it, const PktHdr &h, const char *from) {
  if (!relayOn || h.ttl == 0 || (h.type != T_MSG && h.type != T_ACK)) return;
  if (relayedBefore(h, from)) return;
  for (auto &r : relayq) {
    if (r.used) continue;
    memcpy(r.buf, it.data, it.len);
    r.buf[offsetof(PktHdr, ttl)] = h.ttl - 1;
    r.len = it.len;
    r.at = millis() + 5 + esp_random() % 45;   // random delay so repeaters don't talk over each other
    r.used = true;
    return;
  }
}

bool chanSeenBefore(uint32_t mid, const char *from) {
  for (auto &s : chanSeen) if (s.mid == mid && !strcasecmp(s.from, from)) return true;
  Seen &s = chanSeen[chanSeenPos];
  chanSeenPos = (chanSeenPos + 1) % 48;
  s.mid = mid;
  strlcpy(s.from, from, sizeof s.from);
  return false;
}

void handleChanMsg(int chi, const PktHdr &h, const RxItem &it, const char *from) {
  if (chanSeenBefore(h.mid, from)) return;
  const uint8_t *pl = it.data + HDR_LEN;
  int pn = it.len - HDR_LEN;
  uint8_t pt[200];
  int n;
  bool enc = h.flags & F_ENC;
  if (enc != chrt[chi].hasKey) return;   // open/private mismatch: not really our channel
  if (enc) {
    n = pn - IV_LEN - TAG_LEN;
    if (n < 2 || n > (int)sizeof pt) return;
    if (!gcmDecrypt(chrt[chi].key, it.data, pl, pl + IV_LEN, pl + IV_LEN + TAG_LEN, n, pt)) return;
  } else {
    n = pn;
    if (n < 2 || n > (int)sizeof pt) return;
    memcpy(pt, pl, n);
  }
  int nl = pt[0];
  if (nl > 23 || 1 + nl >= n) return;
  Msg *m = newMsg();
  m->kind = 1;
  m->dir = 0;
  m->st = 10;
  m->enc = enc;
  m->mid = h.mid;
  strlcpy(m->peer, chrt[chi].addr, sizeof m->peer);
  strlcpy(m->from, from, sizeof m->from);
  cleanText(m->fromNick, sizeof m->fromNick, (const char *)pt + 1, nl, false);
  cleanText(m->text, sizeof m->text, (const char *)pt + 1 + nl, n - 1 - nl, true);
  chrt[chi].unread++;
  chrt[chi].lastAct = millis();
  netChanged = true;
  onIncoming(m);
}

void handlePacket(const RxItem &it) {
  PktHdr h;
  memcpy(&h, it.data, HDR_LEN);
  if (h.ver != PROTO_VER) return;
  h.from[19] = 0; h.to[19] = 0;
  char from[20], to[20];
  if (!normalizeId(String(h.from), from)) return;
  if (!strcasecmp(from, myId)) return;   // our own packet echoed back by a repeater
  heardCount++;
  const uint8_t *pl = it.data + HDR_LEN;
  int pn = it.len - HDR_LEN;
  uint32_t now = millis();

  // remember who's in direct range (beacons, or messages that haven't been through a repeater)
  bool direct = h.type == T_BEACON || h.ttl == MAX_HOPS;
  Nearby *nb = findNearby(from);
  if (direct && !nb) {
    Nearby *slot = nullptr;
    for (auto &n : nearby) if (!n.used || now - n.seen > NEARBY_TTL_MS) { slot = &n; break; }
    if (!slot) { slot = &nearby[0]; for (auto &n : nearby) if (n.seen < slot->seen) slot = &n; }
    memset(slot, 0, sizeof(Nearby));
    slot->used = true;
    strlcpy(slot->id, from, sizeof slot->id);
    nb = slot;
    netChanged = true;
  }
  if (direct) { nb->seen = now; nb->rssi = it.rssi; }

  if (h.type == T_BEACON) {
    char nick[24];
    cleanText(nick, sizeof nick, (const char *)pl, min(pn, 23), false);
    if (strcmp(nick, nb->nick)) { strlcpy(nb->nick, nick, sizeof nb->nick); netChanged = true; }
    return;
  }
  if (h.type != T_MSG && h.type != T_ACK) return;
  if (!normalizeId(String(h.to), to)) return;
  bool forMe = !strcasecmp(to, myId);
  if (!forMe) maybeRelay(it, h, from);   // channel messages always go on, even if we're in the channel

  if (h.type == T_MSG && (h.flags & F_CHAN)) {
    int chi = findChanByAddr(to);
    if (chi >= 0 && isSetup) handleChanMsg(chi, h, it, from);
    return;
  }
  if (!forMe || !isSetup) return;

  if (h.type == T_ACK) {
    if (pn < 1) return;
    for (auto &o : outq) {
      if (!o.used || o.chan || o.mid != h.mid) continue;
      if (strcasecmp(((PktHdr *)o.buf)->to, from)) continue;
      setMsgStatus(o.uid, pl[0] ? 3 : 1);
      o.used = false;
    }
    return;
  }

  // direct message for us
  for (auto &s : seen) {   // duplicate (our receipt got lost and they resent): receipt again
    if (s.mid == h.mid && !strcasecmp(s.from, from)) { sendAck(from, h.mid, s.ackSt, ++s.ackTries); return; }
  }
  int ci = findContact(from);
  if (ci < 0) {
    ci = addContact(from, nb ? nb->nick : "");
    if (ci < 0) return;                 // contact list full: ignore strangers
    crt[ci].isNew = true;
    saveContacts();
  }
  Msg *m = newMsg();
  m->kind = 0;
  m->dir = 0;
  m->mid = h.mid;
  strlcpy(m->peer, contacts[ci].id, sizeof m->peer);
  strlcpy(m->from, from, sizeof m->from);
  uint8_t ackSt = 0;
  if (h.flags & F_ENC) {
    m->enc = 1;
    int n = pn - IV_LEN - TAG_LEN;
    uint8_t pt[MAX_TEXT + 1];
    if (n > 0 && n <= MAX_TEXT && crt[ci].hasKey &&
        gcmDecrypt(crt[ci].key, it.data, pl, pl + IV_LEN, pl + IV_LEN + TAG_LEN, n, pt)) {
      cleanText(m->text, sizeof m->text, (const char *)pt, n, true);
      m->st = 10;
    } else {
      m->st = 11;
      ackSt = 1;
    }
  } else {
    cleanText(m->text, sizeof m->text, (const char *)pl, min(pn, MAX_TEXT), true);
    m->st = 10;
  }
  crt[ci].unread++;
  crt[ci].lastAct = millis();
  Seen &s = seen[seenPos];
  seenPos = (seenPos + 1) % 32;
  s.mid = h.mid; s.ackSt = ackSt; s.ackTries = 0;
  strlcpy(s.from, from, sizeof s.from);
  sendAck(from, h.mid, ackSt, 0);
  netChanged = true;
  Serial.printf("[rx] %s: %s\n", from, m->st == 10 ? m->text : "(locked)");
  onIncoming(m);
}

// ---------- sending ----------
static Out *freeOut() {
  for (auto &x : outq) if (!x.used) return &x;
  return nullptr;
}

// returns nullptr on success, or a message for the user
const char *sendDirect(int ci, const char *text) {
  if (ci < 0 || ci >= nContacts) return "Unknown contact.";
  Out *o = freeOut();
  if (!o) return "Still sending earlier messages. Try again in a moment.";
  char clean[MAX_TEXT + 1];
  cleanText(clean, sizeof clean, text, strlen(text), true);
  size_t n = strlen(clean);
  if (!n) return "Type a message first.";
  uint32_t mid = esp_random();
  fillHdr(o->buf, T_MSG, contacts[ci].id, mid);
  if (crt[ci].hasKey) {
    ((PktHdr *)o->buf)->flags = F_ENC;   // set before encrypting: the header is authenticated
    uint8_t *iv = o->buf + HDR_LEN, *tag = iv + IV_LEN, *ct = tag + TAG_LEN;
    if (!gcmEncrypt(crt[ci].key, o->buf, (const uint8_t *)clean, n, iv, tag, ct)) return "Encryption failed.";
    o->len = HDR_LEN + IV_LEN + TAG_LEN + n;
  } else {
    memcpy(o->buf + HDR_LEN, clean, n);
    o->len = HDR_LEN + n;
  }
  Msg *m = newMsg();
  m->kind = 0;
  m->dir = 1;
  m->st = 0;
  m->mid = mid;
  m->enc = crt[ci].hasKey;
  strlcpy(m->peer, contacts[ci].id, sizeof m->peer);
  strlcpy(m->text, clean, sizeof m->text);
  o->used = true;
  o->chan = false;
  o->mid = mid;
  o->uid = m->uid;
  o->tries = 0;
  o->next = millis();
  crt[ci].lastAct = millis();
  netChanged = true;
  return nullptr;
}

const char *sendChannel(int chi, const char *text) {
  if (chi < 0 || chi >= nChans) return "Unknown channel.";
  Out *o = freeOut();
  if (!o) return "Still sending earlier messages. Try again in a moment.";
  char clean[MAX_CHAN_TEXT + 1];
  cleanText(clean, sizeof clean, text, strlen(text), true);
  size_t n = strlen(clean);
  if (!n) return "Type a message first.";
  uint8_t pt[1 + 23 + MAX_CHAN_TEXT];
  size_t nl = strlen(myNick);
  if (nl > 23) nl = 23;
  pt[0] = nl;
  memcpy(pt + 1, myNick, nl);
  memcpy(pt + 1 + nl, clean, n);
  size_t pn = 1 + nl + n;
  uint32_t mid = esp_random();
  fillHdr(o->buf, T_MSG, chrt[chi].addr, mid);
  PktHdr *h = (PktHdr *)o->buf;
  h->flags = F_CHAN;
  if (chrt[chi].hasKey) {
    h->flags |= F_ENC;
    uint8_t *iv = o->buf + HDR_LEN, *tag = iv + IV_LEN, *ct = tag + TAG_LEN;
    if (!gcmEncrypt(chrt[chi].key, o->buf, pt, pn, iv, tag, ct)) return "Encryption failed.";
    o->len = HDR_LEN + IV_LEN + TAG_LEN + pn;
  } else {
    memcpy(o->buf + HDR_LEN, pt, pn);
    o->len = HDR_LEN + pn;
  }
  Msg *m = newMsg();
  m->kind = 1;
  m->dir = 1;
  m->st = 0;
  m->mid = mid;
  m->enc = chrt[chi].hasKey;
  strlcpy(m->peer, chrt[chi].addr, sizeof m->peer);
  strlcpy(m->from, myId, sizeof m->from);
  strlcpy(m->fromNick, myNick, sizeof m->fromNick);
  strlcpy(m->text, clean, sizeof m->text);
  o->used = true;
  o->chan = true;
  o->mid = mid;
  o->uid = m->uid;
  o->tries = 0;
  o->next = millis();
  chrt[chi].lastAct = millis();
  netChanged = true;
  return nullptr;
}

void resendFailed(uint32_t uid) {
  Msg *m = findMsg(uid);
  if (!m || m->dir != 1 || m->st != 2 || m->kind != 0) return;
  int ci = findContact(m->peer);
  if (ci < 0) return;
  char t[MAX_TEXT + 4];
  strlcpy(t, m->text, sizeof t);
  m->st = 99;   // hide the failed copy; the resend becomes a new message
  sendDirect(ci, t);
}

// ---------- setup / loop ----------
void makeId() {
  char suf[8];
  for (int i = 0; i < 4; i++) suf[i] = '0' + esp_random() % 10;
  const char *L = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  for (int i = 4; i < 7; i++) suf[i] = L[esp_random() % 52];
  suf[7] = 0;
  snprintf(myId, sizeof myId, "%s%s", ID_PREFIX, suf);
}

void loadState() {
  prefs.begin("ca53y", false);
  String id = prefs.getString("id", "");
  if (!normalizeId(id, myId)) { makeId(); prefs.putString("id", myId); }
  strlcpy(myNick, prefs.getString("nick", "").c_str(), sizeof myNick);
  isSetup = prefs.getBool("setup", false);
  discoverable = prefs.getBool("disc", true);
  relayOn = prefs.getBool("relay", true);
  soundOn = prefs.getBool("snd", true);
  brightIdx = prefs.getUChar("bri", 2) % 4;
  sleepIdx = prefs.getUChar("slp", 1) % 4;
  size_t blen = prefs.getBytesLength("contacts");
  if (blen && blen % sizeof(Contact) == 0 && blen <= sizeof contacts) {
    prefs.getBytes("contacts", contacts, blen);
    nContacts = blen / sizeof(Contact);
  }
  blen = prefs.getBytesLength("chans");
  if (blen && blen % sizeof(Channel) == 0 && blen <= sizeof chans) {
    prefs.getBytes("chans", chans, blen);
    nChans = blen / sizeof(Channel);
  }
  for (int i = 0; i < nContacts; i++) { memset(&crt[i], 0, sizeof(ContactRt)); deriveKey(i); }
  for (int i = 0; i < nChans; i++) { memset(&chrt[i], 0, sizeof(ChannelRt)); deriveChan(i); }
}

void netSetup() {
  rxq = xQueueCreate(16, sizeof(RxItem));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);   // radio always listening, so we can repeat for others
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed, restarting"); delay(2000); ESP.restart(); }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  Serial.printf("\nCA53Y Comms " DEVICE_KIND " %s  %s  channel %d  max hops %d\n", FW_VERSION, myId, ESPNOW_CHANNEL, MAX_HOPS);
}

void netLoop() {
  RxItem it;
  int n = 0;
  while (n++ < 8 && xQueueReceive(rxq, &it, 0) == pdTRUE) handlePacket(it);

  uint32_t now = millis();

  for (auto &r : relayq) {   // pass along messages meant for someone else
    if (!r.used || (int32_t)(now - r.at) < 0) continue;
    esp_now_send(BCAST, r.buf, r.len);
    r.used = false;
    relayedCount++;
    lastRelayAt = now;
  }

  for (auto &o : outq) {     // send / retry until a receipt comes back
    if (!o.used || (int32_t)(now - o.next) < 0) continue;
    uint8_t limit = o.chan ? CHAN_SENDS : SEND_TRIES;
    if (o.tries >= limit) { setMsgStatus(o.uid, o.chan ? 4 : 2); o.used = false; continue; }
    ((PktHdr *)o.buf)->tries = o.tries;
    esp_now_send(BCAST, o.buf, o.len);
    o.tries++;
    o.next = now + (o.chan ? 700 : RETRY_MS) + esp_random() % 250;
  }

  if (isSetup && discoverable && (int32_t)(now - lastBeacon) > BEACON_MS) {
    lastBeacon = now + esp_random() % 400;   // jitter so devices don't collide
    sendBeacon();
  }

  static uint32_t lastPrune = 0;   // let Nearby forget devices that went quiet
  if (now - lastPrune > 2000) {
    lastPrune = now;
    for (auto &nb : nearby)
      if (nb.used && now - nb.seen > NEARBY_TTL_MS) { nb.used = false; netChanged = true; }
  }
}

// PURE WARDRIVER Swarm node (stage 1). See SwarmNode.h.
// Implements the connected (GATT) link of the Swarm spec:
// hello/time/ack, heartbeats, slot/plan/run/ack/ping, sighting queue
// with ack/delete/resend, TIME:REQUEST clock sync. Owner lock deferred
// (RSP:own:ERROR, never flag 64); plans are stored for hash match while
// the EU/US wardrive schedule keeps running.

#include "SwarmNode.h"
#include "settings.h"
#include "WiFiScan.h"
#ifdef HAS_SCREEN
#include "MenuFunctions.h"
#include "Display.h"
#endif
#include <Preferences.h>
#ifdef HAS_BT
#include <NimBLEDevice.h>
#endif

SwarmNode swarm_obj;
static SwarmNode* g_swarm = nullptr;

static const char* SWARM_SVC_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static const char* SWARM_CMD_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
static const char* SWARM_RSP_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";
static const char* SWARM_STA_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26aa";

static const uint32_t SWARM_BEAT_MS = 3000;
static const uint32_t SWARM_TIMEREQ_MS = 5000;
static const uint32_t SWARM_HELLO_BEAT_MS = 4000;
static const uint32_t SWARM_UNPROVEN_DROP_MS = 15000;
static const uint32_t SWARM_ACK_STALL_MS = 10000;
static const uint8_t SWARM_PUMP_PER_TICK = 4;
static const uint16_t SWARM_NOTIFY_SAFE = 200;

#ifdef HAS_BT
static NimBLEServer* s_pServer = nullptr;
static NimBLECharacteristic* s_pCmd = nullptr;
static NimBLECharacteristic* s_pResp = nullptr;
static NimBLECharacteristic* s_pStat = nullptr;

#ifdef HAS_NIMBLE_2
class SwarmServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    if (g_swarm) SwarmNode::onServerEvent(true, connInfo.getConnHandle());
    (void)pServer;
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.println(String(F("[SWARM] phone disconnected 0x")) + String(reason, HEX));
    if (g_swarm) SwarmNode::onServerEvent(false, connInfo.getConnHandle());
    (void)pServer;
  }
};

class SwarmCmdCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    if (!g_swarm) return;
    const NimBLEAttValue& v = pChar->getValue();
    SwarmNode::onCommandWrite(v.data(), v.length());
    (void)connInfo;
  }
};
#else
// NimBLE 1.x (CP-ADV core 2.0.11): same roles, older signatures.
class SwarmServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
    if (g_swarm && desc != nullptr) SwarmNode::onServerEvent(true, desc->conn_handle);
    (void)pServer;
  }
  void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
    uint16_t h = (desc != nullptr) ? desc->conn_handle : BLE_HS_CONN_HANDLE_NONE;
    Serial.println(F("[SWARM] phone disconnected"));
    if (g_swarm) SwarmNode::onServerEvent(false, h);
    (void)pServer;
  }
};

class SwarmCmdCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    if (!g_swarm) return;
    std::string v = pChar->getValue();
    SwarmNode::onCommandWrite((const uint8_t*)v.data(), v.length());
  }
};
#endif
#endif

// ---------------------------------------------------------------------------
// Helpers (spec algorithms; test vectors: plan 1.25/6.25/11.25 -> 8195)
// ---------------------------------------------------------------------------

uint16_t SwarmNode::planHash(uint8_t flags, const uint8_t* ch,
                             const uint8_t* dwell, uint8_t n) {
  uint32_t h = 2166136261u;
  h = (h ^ (uint8_t)(flags & 3)) * 16777619u;
  h = (h ^ n) * 16777619u;
  for (uint8_t i = 0; i < n; i++) {
    h = (h ^ ch[i]) * 16777619u;
    h = (h ^ dwell[i]) * 16777619u;
  }
  return (uint16_t)((h >> 16) ^ (h & 0xFFFF));
}

String SwarmNode::swarmSecurity(int authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
    default: return "UNKNOWN";
  }
}

static int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

static String swarmSanitize(const String& in) {
  String out;
  out.reserve(25);
  for (size_t i = 0; i < in.length() && out.length() < 24; i++) {
    char c = in.charAt(i);
    if (c == ',' || c == '\r' || c == '\n' || c == '\0')
      out += '_';
    else
      out += c;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void SwarmNode::begin() {
  g_swarm = this;
  loadPersist();
  _begun = true;
}

bool SwarmNode::isEnabled() {
  return settings_obj.loadSetting<bool>("Swarm");
}

SwarmNode::LinkState SwarmNode::linkState() {
  if (!isEnabled()) return SWARM_OFF;
  if (_connected && _proven) return SWARM_LINKED;
  return SWARM_ADVERTISING;
}

void SwarmNode::loadPersist() {
  Preferences prefs;
  if (!prefs.begin("swarm", true)) return;
  _node = prefs.getUChar("node", 0);
  _plan = prefs.getString("plan", "");
  _runMask = prefs.getUChar("runm", 0);
  _runSession = prefs.getUInt("runs", 0);
  _seqReserved = prefs.getUInt("seqrsv", SWARM_SEQ_RESERVE);
  prefs.end();
  if (_seqReserved < SWARM_SEQ_RESERVE) _seqReserved = SWARM_SEQ_RESERVE;
  _seqNext = _seqReserved;
  _ackedBefore = _seqNext;
  _sendCursor = _seqNext;
}

void SwarmNode::saveNode() {
  Preferences prefs;
  if (!prefs.begin("swarm", false)) return;
  prefs.putUChar("node", _node);
  prefs.end();
}

void SwarmNode::savePlan() {
  Preferences prefs;
  if (!prefs.begin("swarm", false)) return;
  prefs.putString("plan", _plan);
  prefs.end();
}

void SwarmNode::saveRun() {
  Preferences prefs;
  if (!prefs.begin("swarm", false)) return;
  prefs.putUChar("runm", _runMask);
  prefs.putUInt("runs", _runSession);
  prefs.end();
}

void SwarmNode::reserveSeq() {
  Preferences prefs;
  if (!prefs.begin("swarm", false)) return;
  _seqReserved = _seqNext + SWARM_SEQ_RESERVE;
  prefs.putUInt("seqrsv", _seqReserved);
  prefs.end();
}

String SwarmNode::advName() {
  if (_node == 0) return "WDU";
  return "WDN" + String(_node);
}

// ---------------------------------------------------------------------------
// BLE plumbing
// ---------------------------------------------------------------------------

bool SwarmNode::ensureBle() {
#ifdef HAS_BT
  if (!isEnabled()) {
    teardownBle();
    return false;
  }
#ifdef HAS_NIMBLE_2
  if (!NimBLEDevice::isInitialized()) {
    // Nobody brought the stack up (e.g. swarm enabled without wardrive):
    // bring it up ourselves. init() is idempotent, wardrive reuses it.
    // Stack teardown (scan shutdown) lands here too — drop dead handles,
    // rebuild below.
    s_pServer = nullptr;
    s_pCmd = nullptr;
    s_pResp = nullptr;
    s_pStat = nullptr;
    _bleUp = false;
    _connected = false;
    _proven = false;
    if (!NimBLEDevice::init("WDU")) return false;
  }
#else
  // NimBLE 1.x has no isInitialized(): init() itself is guarded, safe to
  // repeat; a torn-down stack shows as a null server while we think we're up.
  NimBLEDevice::init("WDU");
  if (NimBLEDevice::getServer() == nullptr && _bleUp) {
    s_pServer = nullptr;
    s_pCmd = nullptr;
    s_pResp = nullptr;
    s_pStat = nullptr;
    _bleUp = false;
    _connected = false;
    _proven = false;
  }
#endif
  if (_bleUp && s_pServer != nullptr) return true;
  // Match the reference node: MTU 247 up front (long plan writes).
  NimBLEDevice::setMTU(247);
  s_pServer = NimBLEDevice::createServer();
  if (s_pServer == nullptr) return false;
  static SwarmServerCB serverCB;
  s_pServer->setCallbacks(&serverCB);
  s_pServer->advertiseOnDisconnect(true);
  NimBLEService* pSvc = s_pServer->createService(SWARM_SVC_UUID);
  if (pSvc == nullptr) return false;
  s_pCmd = pSvc->createCharacteristic(SWARM_CMD_UUID,
                                      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
#ifdef HAS_NIMBLE_2
                                      , 512
#endif
                                      );
  s_pResp = pSvc->createCharacteristic(SWARM_RSP_UUID, NIMBLE_PROPERTY::NOTIFY);
  s_pStat = pSvc->createCharacteristic(SWARM_STA_UUID, NIMBLE_PROPERTY::READ);
  if (s_pCmd == nullptr || s_pResp == nullptr || s_pStat == nullptr) return false;
  static SwarmCmdCB cmdCB;
  s_pCmd->setCallbacks(&cmdCB);
  s_pStat->setValue("{\"status\":1,\"battery\":0,\"build\":1}");
  pSvc->start();
  _bleUp = true;
  _connected = false;
  _proven = false;
  Serial.println(String(F("[SWARM] advertising ")) + advName());
  restartAdvertising();
  return true;
#else
  return false;
#endif
}

void SwarmNode::teardownBle() {
#ifdef HAS_BT
  if (s_pServer != nullptr && _connected)
    s_pServer->disconnect(_connHandle);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv != nullptr) adv->stop();
  _connected = false;
  _proven = false;
#endif
}

void SwarmNode::suspendForUpload() {
  if (!isEnabled()) return;
  _suspended = true;
  teardownBle();
#ifdef HAS_BT
  // Full stack down: no advertising, no reconnects mid-upload.
  // update() rebuilds everything once resumeFromUpload() clears the flag.
  NimBLEDevice::deinit();
#endif
  s_pServer = nullptr;
  s_pCmd = nullptr;
  s_pResp = nullptr;
  s_pStat = nullptr;
  _bleUp = false;
  _connected = false;
  _proven = false;
  // update() restarts advertising on its own once the upload is done.
}

void SwarmNode::resumeFromUpload() {
  _suspended = false;
}

void SwarmNode::restartAdvertising() {
#ifdef HAS_BT
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv == nullptr) return;
  adv->stop();
#ifdef HAS_NIMBLE_2
  adv->removeServices();
#else
  adv->reset();
#endif
  adv->addServiceUUID(SWARM_SVC_UUID);
  adv->setName(advName().c_str());
#ifdef HAS_NIMBLE_2
  adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);
  adv->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
#endif
  adv->start();
#endif
}

void SwarmNode::onServerEvent(bool connected, uint16_t connHandle) {
  if (g_swarm == nullptr) return;
  if (connected) {
    g_swarm->_connected = true;
    g_swarm->_connHandle = connHandle;
    g_swarm->_proven = false;
    g_swarm->_linkSilentSince = millis();
    Serial.println(F("[SWARM] phone connected"));
  } else {
    g_swarm->_connected = false;
    g_swarm->_proven = false;
  }
}

void SwarmNode::onCommandWrite(const uint8_t* data, size_t len) {
  if (g_swarm == nullptr || data == nullptr || len == 0) return;
  if (len > 512) len = 512;
  g_swarm->_linkSilentSince = millis();
  g_swarm->_inBuf += String((const char*)data, len);
  if (g_swarm->_inBuf.length() > 512)
    g_swarm->_inBuf = g_swarm->_inBuf.substring(g_swarm->_inBuf.length() - 512);
}

// ---------------------------------------------------------------------------
// Protocol
// ---------------------------------------------------------------------------

static String swarmArg(const String& args, const String& key) {
  int start = 0;
  while (start < (int)args.length()) {
    int comma = args.indexOf(',', start);
    String pair = comma < 0 ? args.substring(start) : args.substring(start, comma);
    int eq = pair.indexOf('=');
    if (eq > 0 && pair.substring(0, eq) == key)
      return pair.substring(eq + 1);
    if (comma < 0) break;
    start = comma + 1;
  }
  return "";
}

void SwarmNode::handleLine(const String& line) {
  int c1 = line.indexOf(':');
  if (c1 <= 0) return;
  int c2 = line.indexOf(':', c1 + 1);
  if (c2 <= c1) return;
  String type = line.substring(0, c1);
  String sub = line.substring(c1 + 1, c2);
  String payload = line.substring(c2 + 1);

  if (type == "TIME" && sub == "SYNC") {
    int Y, Mo, D, h, mi, s;
    if (sscanf(payload.c_str(), "%d-%d-%dT%d:%d:%dZ", &Y, &Mo, &D, &h, &mi, &s) == 6) {
      _clockUnix = (uint32_t)(days_from_civil(Y, (unsigned)Mo, (unsigned)D) * 86400LL + h * 3600 + mi * 60 + s);
      _clockMs = millis();
      _clockSet = true;
    }
    return;  // TIME lines get no RSP and no heartbeat
  }

  if (type != "CMD") return;

  if (!_proven && sub != "hello" && sub != "ping") {
    // Pre-hello (and, without owner lock, only pre-hello): ack + TIME
    // stay silent, everything else is refused.
    if (sub != "ack") sendLine("RSP:" + sub + ":ERROR:owned");
    return;
  }

  if (sub == "ack") {
    uint32_t n = strtoul(swarmArg(payload, "n").c_str(), nullptr, 10);
    if (n > _ackedBefore) {
      while (_qCount > 0 && _queueBaseSeq < n) {
        _qHead = (_qHead + 1) % SWARM_QUEUE_CAP;
        _qCount--;
        _queueBaseSeq++;
      }
      _ackedBefore = n;
      _lastAckSeq = n;
      _ackProgressMs = millis();
      if (_sendCursor < _ackedBefore) _sendCursor = _ackedBefore;
    }
    return;  // acks: no RSP, no heartbeat
  }

  if (sub == "hello") {
    // Stage 1: no owner lock — every hello succeeds.
    _proven = true;
    _linkSilentSince = millis();
    _sendCursor = _ackedBefore;  // upload from oldest unacked
    if (sendLine("RSP:hello:OK"))
      Serial.println(F("[SWARM] hello ok, RSP sent"));
    else
      Serial.println(F("[SWARM] hello ok, RSP FAILED"));
    sendHeartbeat();
    return;
  }

  if (sub == "ping") {
    sendLine("RSP:ping:OK");
    sendHeartbeat();
    return;
  }

  if (sub == "slot") {
    _node = (uint8_t)strtoul(swarmArg(payload, "n").c_str(), nullptr, 10);
    saveNode();
    restartAdvertising();
    Serial.println(String(F("[SWARM] slot node=")) + _node);
    sendLine("RSP:slot:OK");
    sendHeartbeat();
    return;
  }

  if (sub == "plan") {
    _plan = payload;
    if (_plan.length() > 512) _plan = _plan.substring(0, 512);
    savePlan();
    Serial.println(F("[SWARM] plan stored"));
    sendLine("RSP:plan:OK");
    sendHeartbeat();
    return;
  }

  if (sub == "run") {
    uint8_t m = (uint8_t)strtoul(swarmArg(payload, "m").c_str(), nullptr, 10);
    uint32_t s = strtoul(swarmArg(payload, "s").c_str(), nullptr, 10);
    bool changed = (m != _runMask) || (s != _runSession);
    _runMask = m;
    _runSession = s;
    saveRun();
    Serial.println(String(F("[SWARM] run m=")) + m + " s=" + s);
    sendLine("RSP:run:OK");
    if (m == 0) {
      sendStatus("STATUS:1:Ready");
      stopWardriveRemote();
    } else {
      sendStatus("STATUS:2:Busy");
      if (changed) startWardriveRemote();
    }
    sendHeartbeat();
    return;
  }

  if (sub == "own") {
    // Stage 1: owner lock deferred.
    sendLine("RSP:own:ERROR");
    sendHeartbeat();
    return;
  }

  if (sub == "disown") {
    sendLine("RSP:disown:OK");
    sendHeartbeat();
    return;
  }

  sendLine("RSP:" + sub + ":ERROR:unknown command");
  sendHeartbeat();
}

bool SwarmNode::sendLine(const String& line) {
#ifdef HAS_BT
  if (!_connected || !_proven || s_pResp == nullptr) return false;
  String out = line + "\n";
  if (out.length() > 244) out = out.substring(0, 244);
  // Fixed safe chunk: the phone asks MTU 247 right after connect, and a
  // failed notify simply stays queued for the next tick — no loss either way.
  const size_t mtu = SWARM_NOTIFY_SAFE;
  for (size_t off = 0; off < out.length(); off += mtu) {
    size_t chunk = out.length() - off;
    if (chunk > mtu) chunk = mtu;
#ifdef HAS_NIMBLE_2
    // Explicit link handle like the reference node (single phone link).
    if (!s_pResp->notify((const uint8_t*)(out.c_str() + off), chunk, _connHandle)) return false;
#else
    s_pResp->setValue((const uint8_t*)(out.c_str() + off), chunk);
    s_pResp->notify();
#endif
  }
  return true;
#else
  (void)line;
  return false;
#endif
}

void SwarmNode::sendHeartbeat() {
  if (!_connected || !_proven) return;
  bool scanning = (wifi_scan_obj.currentScanMode == WIFI_SCAN_WAR_DRIVE);
  uint8_t f = 0;
  if (scanning) f += 1;
#ifdef HAS_DUAL_BAND
  f += 8;
#endif
  if (_clockSet) f += 32;
#ifdef HAS_GPS
  // Proposal extension: 128 = own GPS hardware present (module answers
  // on UART, onboard or soldered) — independent of fix.
  if (gps_obj.getGpsModuleStatus()) f += 128;
#endif
  // Stage 1: no owner (64) and no flash spill (16) by design.
  uint8_t h_n = 0;
  uint8_t h_ch[48];
  uint8_t h_dw[48];
  parsePlan(h_ch, h_dw, &h_n);
  uint8_t h_f = 0;
  int comma = _plan.indexOf(',');
  if (comma > 0) h_f = (uint8_t)strtoul(_plan.substring(2, comma).c_str(), nullptr, 10);
  uint16_t h = planHash(h_f, h_ch, h_dw, h_n);
  uint8_t m = scanning ? (_runMask ? _runMask : 3) : 0;
  uint32_t t = _qCount > 0 ? _queueBaseSeq : _seqNext;
  String beat = "DATA:BEAT:f=" + String(f) + ",h=" + String(h) + ",m=" + String(m) +
                ",s=" + String(_runSession) + ",n=" + String(_node) + ",b=" + String(_qCount) +
                ",l=" + String(_lost) + ",q=" + String(_seqNext) + ",t=" + String(t) +
                ",v=" + String(SWARM_BUILD);
  sendLine(beat);
}

void SwarmNode::sendTimeRequest() {
  sendLine("TIME:REQUEST:");
}

void SwarmNode::sendStatus(const char* text) {
  sendLine(String(text));
}

void SwarmNode::pumpUpload(uint32_t now) {
  if (!_connected || !_proven || _qCount == 0) return;
  if (_sendCursor < _ackedBefore) _sendCursor = _ackedBefore;
  if (_sendCursor < _queueBaseSeq) _sendCursor = _queueBaseSeq;
  if (_ackedBefore == _lastAckSeq && _qCount > 0 && (now - _ackStallSince) > SWARM_ACK_STALL_MS) {
    _sendCursor = _ackedBefore < _queueBaseSeq ? _queueBaseSeq : _ackedBefore;
    _ackStallSince = now;
  }
  uint8_t sent = 0;
  while (sent < SWARM_PUMP_PER_TICK && _sendCursor < _seqNext) {
    if (_sendCursor < _queueBaseSeq) {
      _sendCursor = _queueBaseSeq;
      continue;
    }
    uint8_t idx = (_qHead + (_sendCursor - _queueBaseSeq)) % SWARM_QUEUE_CAP;
    uint32_t t = sightingUnix(_queue[idx].capMs);
    if (t == 0) break;  // clock not set: hold, keep queued
    if (!sendLine(sightingLine(_queue[idx], t))) break;
    _sendCursor++;
    sent++;
  }
  if (_sendCursor >= _seqNext) _ackStallSince = now;
}

// ---------------------------------------------------------------------------
// Queue + sightings
// ---------------------------------------------------------------------------

void SwarmNode::parsePlan(uint8_t* ch, uint8_t* dwell, uint8_t* n) {
  *n = 0;
  if (_plan.length() == 0) return;
  int comma = _plan.indexOf(',');
  if (comma < 0) return;
  // entries look like "6.25" (channel 6, dwell 25 = 250 ms)
  int start = comma + 1;
  while (start < (int)_plan.length() && *n < 48) {
    int slash = _plan.indexOf('/', start);
    String entry = slash < 0 ? _plan.substring(start) : _plan.substring(start, slash);
    int dot = entry.indexOf('.');
    if (dot > 0) {
      int c = atoi(entry.substring(0, dot).c_str());
      int d = atoi(entry.substring(dot + 1).c_str());
      if (c > 0 && c < 240 && d >= 2 && d <= 255) {
        ch[*n] = (uint8_t)c;
        dwell[*n] = (uint8_t)d;
        (*n)++;
      }
    }
    if (slash < 0) break;
    start = slash + 1;
  }
}

uint32_t SwarmNode::sightingUnix(uint32_t capMs) {
  if (!_clockSet) return 0;
  return _clockUnix + (capMs - _clockMs) / 1000;
}

bool SwarmNode::dedupHit(Dedup* table, uint8_t n, const char* key,
                         uint32_t now, uint32_t windowMs) {
  static uint8_t rrAP = 0;
  static uint8_t rrBT = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (table[i].key[0] != '\0' && strcmp(table[i].key, key) == 0) {
      if (now - table[i].ms < windowMs) return true;
      table[i].ms = now;
      return false;
    }
  }
  uint8_t slot = (table == _dedupAP) ? (rrAP++ % n) : (rrBT++ % n);
  strncpy(table[slot].key, key, sizeof(table[slot].key) - 1);
  table[slot].key[sizeof(table[slot].key) - 1] = '\0';
  table[slot].ms = now;
  return false;
}

void SwarmNode::queueSighting(const Sighting& s) {
  if (_qCount >= SWARM_QUEUE_CAP) {
    // Full: drop oldest, count it (heartbeat l).
    _qHead = (_qHead + 1) % SWARM_QUEUE_CAP;
    _qCount--;
    _queueBaseSeq++;
    _lost++;
  }
  uint8_t idx = (_qHead + _qCount) % SWARM_QUEUE_CAP;
  _queue[idx] = s;
  _qCount++;
  if (_seqNext % SWARM_SEQ_RESERVE == 0) reserveSeq();
}

String SwarmNode::sightingLine(const Sighting& s, uint32_t unixTime) {
  if (s.kind == 0) {
    return "DATA:AP:" + String(s.id1) + "," + String(s.id2) + "," +
           String(s.ch) + "," + String(s.rssi) + ",[" + String(s.sec) +
           "],q=" + String(s.seq) + ",t=" + String(unixTime) +
           ",s=" + String(s.session);
  }
  return "DATA:BT:" + String(s.id1) + "," + String(s.id2) + "," +
         String(s.rssi) + ",BLE,q=" + String(s.seq) + ",t=" +
         String(unixTime) + ",s=" + String(s.session);
}

void SwarmNode::reportAP(const String& ssid, const String& bssid,
                         uint8_t channel, int rssi, int authmode) {
  if (!isEnabled() || _runMask == 0) return;
  uint32_t now = millis();
  String mac = bssid;
  mac.toLowerCase();
  if (dedupHit(_dedupAP, SWARM_DEDUP_AP, mac.c_str(), now, 300000)) return;
  Sighting s;
  s.seq = _seqNext++;
  s.capMs = now;
  s.kind = 0;
  strncpy(s.id1, swarmSanitize(ssid).c_str(), sizeof(s.id1) - 1);
  strncpy(s.id2, mac.c_str(), sizeof(s.id2) - 1);
  s.ch = channel;
  s.rssi = (rssi < -128) ? -128 : (int8_t)rssi;
  strncpy(s.sec, swarmSecurity(authmode).c_str(), sizeof(s.sec) - 1);
  s.session = _runSession;
  queueSighting(s);
}

void SwarmNode::reportBT(const String& name, const String& mac, int rssi) {
  if (!isEnabled() || _runMask == 0) return;
  uint32_t now = millis();
  String m = mac;
  m.toLowerCase();
  if (dedupHit(_dedupBT, SWARM_DEDUP_BT, m.c_str(), now, 30000)) return;
  Sighting s;
  s.seq = _seqNext++;
  s.capMs = now;
  s.kind = 1;
  strncpy(s.id1, swarmSanitize(name).c_str(), sizeof(s.id1) - 1);
  strncpy(s.id2, m.c_str(), sizeof(s.id2) - 1);
  s.ch = 0;
  s.rssi = (rssi < -128) ? -128 : (int8_t)rssi;
  s.session = _runSession;
  queueSighting(s);
}

// ---------------------------------------------------------------------------
// Remote scan control (mirrors the manual SCAN/STOP UI paths)
// ---------------------------------------------------------------------------

void SwarmNode::stopWardriveRemote() {
  wifi_scan_obj.StartScan(WIFI_SCAN_OFF, TFT_RED);
  wifi_scan_obj.suppress_wardrive_stats = false;
#ifdef HAS_SCREEN
  menu_function_obj.show_home_during_scan = false;
  display_obj.clearScreen();
  display_obj.showCenterText("Wardriving stopped", TFT_HEIGHT / 2, true);
  delay(1000);
  menu_function_obj.changeMenu(menu_function_obj.current_menu, true);
#endif
}

void SwarmNode::startWardriveRemote() {
#ifdef HAS_SCREEN
  display_obj.clearScreen();
  menu_function_obj.drawStatusBar();
#endif
  wifi_scan_obj.StartScan(WIFI_SCAN_WAR_DRIVE, TFT_BLUE);
}

// ---------------------------------------------------------------------------
// Main tick (called from loop())
// ---------------------------------------------------------------------------

void SwarmNode::main(uint32_t now) {
  if (!_begun) begin();
  if (_suspended) {
    // Upload running: keep BLE down even if the phone knocks.
    if (_connected && s_pServer != nullptr) {
#ifdef HAS_BT
      s_pServer->disconnect(_connHandle);
#endif
      _connected = false;
      _proven = false;
    }
    return;
  }
  if (!isEnabled()) {
    teardownBle();
    _bleUp = false;
    return;
  }
  if (!ensureBle()) return;

#ifdef HAS_BT
  // Advertising watchdog: re-assert our advert if anything else stopped it
  // (scan shutdown, stack rebuild) while no phone is connected.
  if (!_connected) {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv != nullptr && !adv->isAdvertising()) restartAdvertising();
  }
#endif

  // Inbound lines.
  int nl;
  while ((nl = _inBuf.indexOf('\n')) >= 0) {
    String line = _inBuf.substring(0, nl);
    line.trim();
    _inBuf = _inBuf.substring(nl + 1);
    if (line.length() > 0) handleLine(line);
  }

  // Drop unproven silent links (pre-hello only matters; proven links idle
  // silently when there is no backlog, by design).
  if (_connected && !_proven && (now - _linkSilentSince) > SWARM_UNPROVEN_DROP_MS) {
#ifdef HAS_BT
    if (s_pServer != nullptr) s_pServer->disconnect(_connHandle);
#endif
    _connected = false;
    return;
  }

  // Boot resume: a persisted phone run survives reboots (spec). Only once
  // per boot and only when idle — never hijack an active session.
  static bool bootResumed = false;
  if (!bootResumed) {
    bootResumed = true;
    if (_runMask != 0 && wifi_scan_obj.currentScanMode == WIFI_SCAN_OFF)
      startWardriveRemote();
  }

  // Heartbeat every 3 s once proven.
  if (_connected && _proven && (now - _lastBeatMs) >= SWARM_BEAT_MS) {
    _lastBeatMs = now;
    sendHeartbeat();
    // Proposal extension: node GPS for the phone. Sent while our own fix
    // is valid; the app gates START on it (5 min phone fallback).
    bool fix = false;
#ifdef HAS_GPS
    fix = gps_obj.getGpsModuleStatus() && gps_obj.getFixStatus();
#endif
    if (fix != _lastFixReported) {
      _lastFixReported = fix;
      Serial.println(fix ? F("[SWARM] GPS fix ok") : F("[SWARM] GPS fix lost"));
    }
    if (fix && _clockSet) {
      String g = "DATA:GPS:" + gps_obj.getLat() + "," + gps_obj.getLon() +
                 "," + String(gps_obj.getAccuracy(), 1) + "," +
                 String(sightingUnix(now));
      sendLine(g);
    }
  }

  // Clock request until the phone sets it.
  if (_connected && !_clockSet && (now - _lastTimeReqMs) >= SWARM_TIMEREQ_MS) {
    _lastTimeReqMs = now;
    sendTimeRequest();
  }

  // Upload pump.
  pumpUpload(now);

  // Keep the Status characteristic fresh.
  if ((now - _lastStatusMs) >= 1000) {
    _lastStatusMs = now;
#ifdef HAS_BT
    if (s_pStat != nullptr) {
      bool scanning = (wifi_scan_obj.currentScanMode == WIFI_SCAN_WAR_DRIVE);
      String js = "{\"status\":" + String(scanning ? 2 : 1) + ",\"battery\":0,\"build\":" +
                  String(SWARM_BUILD) + "}";
      s_pStat->setValue(js.c_str());
    }
#endif
  }
}

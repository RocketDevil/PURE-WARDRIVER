#pragma once

// PURE WARDRIVER Swarm node (stage 1): BLE GATT peripheral speaking the
// Swarm text-line protocol to the Swarm phone app (star topology).
// Settings toggle "Swarm" owns the feature; the phone owns scanning.
// Owner lock intentionally deferred (answers RSP:own:ERROR, no flag 64);
// channel plans are stored (hash matches, phone shows IN SYNC) but the
// EU/US wardrive schedule keeps running for now.

#include "configs.h"
#include <Arduino.h>

class WiFiScan;
class MenuFunctions;

extern WiFiScan wifi_scan_obj;
extern MenuFunctions menu_function_obj;

class SwarmNode {
public:
  // Link state for the home-screen indicator.
  enum LinkState : uint8_t {
    SWARM_OFF = 0,      // feature disabled in settings
    SWARM_ADVERTISING,  // enabled, waiting for the phone
    SWARM_LINKED        // phone connected + hello accepted
  };

  void begin();
  void main(uint32_t now);

  bool isEnabled();
  LinkState linkState();

  // Sighting hooks (called from the wardrive capture paths).
  void reportAP(const String& ssid, const String& bssid, uint8_t channel,
                int rssi, int authmode);
  void reportBT(const String& name, const String& mac, int rssi);

  // Spec helpers (public for unit sanity, match the reference vectors).
  static uint16_t planHash(uint8_t flags, const uint8_t* ch,
                           const uint8_t* dwell, uint8_t n);
  static String swarmSecurity(int authmode);

  // BLE callback entry points (public so the NimBLE callbacks can reach them).
  static void onServerEvent(bool connected, uint16_t connHandle);
  static void onCommandWrite(const uint8_t* data, size_t len);

private:
  static const uint8_t SWARM_BUILD = 1;
  static const uint8_t SWARM_QUEUE_CAP = 128;
  static const uint8_t SWARM_DEDUP_AP = 64;
  static const uint8_t SWARM_DEDUP_BT = 32;
  static const uint32_t SWARM_SEQ_RESERVE = 256;

  struct Sighting {
    uint32_t seq = 0;
    uint32_t capMs = 0;
    uint8_t kind = 0;  // 0 = AP, 1 = BT
    char id1[25] = {0};
    char id2[18] = {0};
    uint8_t ch = 0;
    int8_t rssi = 0;
    char sec[16] = {0};
    uint32_t session = 0;
  };
  struct Dedup {
    char key[18] = {0};
    uint32_t ms = 0;
  };

  // Persistent (NVS "swarm"): node, plan, run mask/session, seq high-water.
  void loadPersist();
  void saveNode();
  void savePlan();
  void saveRun();
  void reserveSeq();

  // BLE plumbing.
  bool ensureBle();
  void teardownBle();
  void restartAdvertising();

  // Protocol.
  void handleLine(const String& line);
  void sendHeartbeat();
  void sendTimeRequest();
  void sendStatus(const char* text);
  void pumpUpload(uint32_t now);
  bool sendLine(const String& line);
  void queueSighting(const Sighting& s);
  String sightingLine(const Sighting& s, uint32_t unixTime);
  bool dedupHit(Dedup* table, uint8_t n, const char* key, uint32_t now,
                uint32_t windowMs);
  uint32_t sightingUnix(uint32_t capMs);
  void parsePlan(uint8_t* ch, uint8_t* dwell, uint8_t* n);
  void stopWardriveRemote();
  void startWardriveRemote();
  String advName();

  bool _begun = false;
  bool _bleUp = false;
  bool _connected = false;
  bool _proven = false;  // hello accepted on this link
  uint16_t _connHandle = 0;
  String _inBuf;
  bool _rxStallNoticed = false;
  uint32_t _linkSilentSince = 0;

  uint8_t _node = 0;
  String _plan;  // raw plan line payload, verbatim
  uint8_t _runMask = 0;
  uint32_t _runSession = 0;
  uint32_t _seqNext = 1;
  uint32_t _seqReserved = SWARM_SEQ_RESERVE;

  bool _clockSet = false;
  uint32_t _clockUnix = 0;
  uint32_t _clockMs = 0;

  Sighting _queue[SWARM_QUEUE_CAP];
  uint8_t _qHead = 0;
  uint8_t _qCount = 0;
  uint32_t _queueBaseSeq = 1;  // seq of _queue[_qHead]
  uint32_t _ackedBefore = 1;  // phone has everything before this seq
  uint32_t _lastAckSeq = 1;
  uint32_t _ackProgressMs = 0;
  uint32_t _lost = 0;
  uint32_t _sendCursor = 1;  // oldest seq not yet (re)sent this link
  uint32_t _ackStallSince = 0;

  Dedup _dedupAP[SWARM_DEDUP_AP];
  Dedup _dedupBT[SWARM_DEDUP_BT];

  uint32_t _lastBeatMs = 0;
  uint32_t _lastTimeReqMs = 0;
  uint32_t _lastStatusMs = 0;
  bool _lastFixReported = false;
};

extern SwarmNode swarm_obj;

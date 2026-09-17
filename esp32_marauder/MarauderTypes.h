#pragma once

// Pure Wardrive: shared wardrive data types. These structs were previously
// defined in EvilPortal.h; they are needed across WiFiScan, MenuFunctions
// and CommandLine independent of the (removed) EvilPortal module.

#ifndef MarauderTypes_h
#define MarauderTypes_h

#include <Arduino.h>
#include <LinkedList.h>

struct ssid {
  String essid;
  uint8_t channel;
  uint8_t bssid[6];
  bool selected;
};

struct AccessPoint {
  String essid;
  uint8_t channel;
  uint8_t bssid[6];
  bool selected;
 // LinkedList<char>* beacon;
  char beacon[2];
  int8_t rssi;
  LinkedList<uint16_t>* stations;
  uint16_t packets;
  uint8_t sec;
  bool wps;
  String man;
  bool has_msg_1;
  bool has_msg_2;
  bool has_msg_3;
  bool has_msg_4;
  uint32_t last_seen_ms;
};

#endif

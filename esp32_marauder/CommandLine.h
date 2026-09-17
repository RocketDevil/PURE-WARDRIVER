#pragma once

#ifndef CommandLine_h
#define CommandLine_h

#include "configs.h"

#ifdef HAS_SCREEN
  #include "MenuFunctions.h"
  #include "Display.h"
#endif 

#include "WiFiScan.h"
//#include "Web.h"
#ifdef HAS_SD
  #include "SDInterface.h"
#endif
#include "settings.h"
// Pure Wardrive: ReconMission removed.
#if defined(HAS_NEOPIXEL_LED)
  #include "LedInterface.h"
#endif

#ifdef HAS_SCREEN
  extern MenuFunctions menu_function_obj;
  extern Display display_obj;
#endif

extern WiFiScan wifi_scan_obj;
//extern Web web_obj;
#ifdef HAS_SD
  extern SDInterface sd_obj;
#endif
extern Settings settings_obj;
// Pure Wardrive: ReconMission removed.
#if defined(HAS_NEOPIXEL_LED)
  extern LedInterface led_obj;
#endif
extern LinkedList<AccessPoint>* access_points;
extern LinkedList<BleDevice>* ble_devices;
extern LinkedList<AirTag>* airtags;
extern LinkedList<Flipper>* flippers;
extern LinkedList<ssid>* ssids;
extern LinkedList<Station>* stations;
extern LinkedList<IPAddress>* ipList;
extern LinkedList<ProbeReqSsid>* probe_req_ssids;
extern const String PROGMEM version_number;
extern const String PROGMEM board_target;

//// Commands

// Admin
const char PROGMEM CH_CMD[] = "channel";
const char PROGMEM REBOOT_CMD[] = "reboot";
const char PROGMEM UPDATE_CMD[] = "update";
const char PROGMEM HELP_CMD[] = "help";
const char PROGMEM SETTINGS_CMD[] = "settings";
const char PROGMEM GEOFENCE_CMD[] = "geofence";
const char PROGMEM LS_CMD[] = "ls";
const char PROGMEM PROTOCOL_INFO_CMD[] = "protocolinfo";
const char PROGMEM BACKUP_SPIFFS_CMD[] = "backupspiffs";
const char PROGMEM BACKUP_STATUS_CMD[] = "backupstatus";
const char PROGMEM RESTORE_SPIFFS_CMD[] = "restorespiffs";
const char PROGMEM LED_CMD[] = "led";
const char PROGMEM GPS_DATA_CMD[] = "gpsdata";
const char PROGMEM GPS_CMD[] = "gps";
const char PROGMEM NMEA_CMD[] = "nmea";
const char PROGMEM GPS_POI_CMD[] = "gpspoi";
const char PROGMEM GPS_TRACKER_CMD[] = "gpstracker";
const char PROGMEM RECON_CMD[] = "recon";

// Pure Wardrive: only wardrive/stopscan/upload/join commands kept
const char PROGMEM STOPSCAN_CMD[] = "stopscan";
const char PROGMEM WARDRIVE_CMD[] = "wardrive";

// WiFi Aux
const char PROGMEM JOIN_CMD[] = "join";
const char PROGMEM UPLOAD_CMD[] = "upload";

// POI
const char PROGMEM WARDRIVEPOI_CMD[] = "wardrivepoi";

//// Command help messages
// Admin
const char PROGMEM HELP_HEAD[] = "============ Commands ============";
const char PROGMEM HELP_CH_CMD[] = "channel [-s <channel>]";
const char PROGMEM HELP_REBOOT_CMD[] = "reboot";
const char PROGMEM HELP_UPDATE_CMD_A[] = "update -s/-w";
const char PROGMEM HELP_SETTINGS_CMD[] = "settings [-s <setting> enable/disable>]/[-r]";
const char PROGMEM HELP_LS_CMD[] = "ls <directory>";
const char PROGMEM HELP_PROTOCOL_INFO_CMD[] = "protocolinfo [--machine <transaction-id>]";
const char PROGMEM HELP_BACKUP_SPIFFS_CMD[] = "backupspiffs [--machine <transaction-id>] - copy SPIFFS to /spiffs on SD";
const char PROGMEM HELP_BACKUP_STATUS_CMD[] = "backupstatus [--machine <transaction-id>] - inspect /spiffs on SD";
const char PROGMEM HELP_RESTORE_SPIFFS_CMD[] = "restorespiffs [--machine <transaction-id>] - restore SPIFFS from /spiffs on SD";
const char PROGMEM HELP_LED_CMD[] = "led -s <hex color>/-p <rainbow>";
const char PROGMEM HELP_GPS_DATA_CMD[] = "gpsdata";
const char PROGMEM HELP_GPS_CMD[] = "gps [-t] [-g] <fix/sat/lon/lat/alt/date/accuracy/text/nmea>\r\n    [-n] <native/all/gps/glonass/galileo/navic/qzss/beidou>\r\n         [-b = use BD vs GB for beidou]";
const char PROGMEM HELP_GPS_TRACKER_CMD[] = "gpstracker -c <start/stop>";
const char PROGMEM HELP_NMEA_CMD[] = "nmea";

// Pure Wardrive: pentest help removed
const char PROGMEM HELP_STOPSCAN_CMD[] = "stopscan [-f]";
const char PROGMEM HELP_WARDRIVE_CMD[] = "wardrive";
const char PROGMEM HELP_GEOFENCE_CMD[] = "geofence list/set <1-5> <lat> <lon> <0.1-1.0 mi> \"<name>\"/clear <1-5>";

// WiFi Aux
const char PROGMEM HELP_JOIN_CMD[] = "join -a <index> -p <password>/-s";
const char PROGMEM HELP_UPLOAD_CMD[] = "upload -d <wdg/wigle/both>";

const char PROGMEM BRIGHTNESS_CMD[] = "brightness";
const char PROGMEM HELP_BRIGHTNESS_CMD[] = "brightness [-c cycle] [-s <0-9>]";

const char PROGMEM HELP_FOOT[] = "==================================";


class CommandLine {
  private:
    String getSerialInput();
    LinkedList<String> parseCommand(String input, char* delim);
    String toLowerCase(String str);
    void filterAccessPoints(String filter);
    bool checkValueExists(LinkedList<String>* cmd_args_list, int index);
    bool inRange(int max, int index);
    //bool apSelected();
    bool hasSSIDs();
    void showCounts(int selected, int unselected = -1);
    int argSearch(LinkedList<String>* cmd_args, const char* key);
    void startScanFromCLI(int scan_mode, uint16_t color, const char* scan_name);

    #ifndef MARAUDER_V8
    const char* ascii_art =
    "\r\n"
    "              @@@@@@                        \r\n"
    "              @@@@@@@@                      \r\n"
    "              @@@@@@@@@@@                   \r\n"
    "             @@@@@@  @@@@@@                 \r\n"
    "          @@@@@@@      @@@@@@@              \r\n"
    "        @@@@@@            @@@@@@            \r\n"
    "     @@@@@@@                @@@@@@@         \r\n"
    "   @@@@@@                      @@@@@@       \r\n"
    "@@@@@@@              @@@@@@@@@@@@@@@@       \r\n"
    "@@@@@                 @@@@@@@@@@@@@@@       \r\n"
    "@@@@@                   @@@@@@@             \r\n"
    "@@@@@                      @@@@@@           \r\n"
    "@@@@@@                       @@@@@@@        \r\n"
    "  @@@@@@                        @@@@@@@@@@@@\r\n"
    "    @@@@@@@                          @@@@@@ \r\n"
    "       @@@@@@                     @@@@@@    \r\n"
    "         @@@@@@@                @@@@@@      \r\n"
    "            @@@@@@           @@@@@@         \r\n"
    "              @@@@@@@      @@@@@@           \r\n"
    "                 @@@@@@ @@@@@@              \r\n"
    "                   @@@@@@@@@                \r\n"
    "                      @@@@@@                \r\n"
    "                        @@@@                \r\n"
    "\r\n";
    #endif
        
  public:

    void RunSetup();
    void main(uint32_t currentTime);
    void runCommand(String input);
};

#endif

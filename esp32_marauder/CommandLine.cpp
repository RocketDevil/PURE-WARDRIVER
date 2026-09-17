#include "CommandLine.h"

// GCOVR_EXCL_START -- serial protocol output depends on Arduino Serial.
namespace {
  bool parseFiniteNumber(const String& value, double& parsed) {
    char* end = nullptr;
    parsed = strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && isfinite(parsed);
  }
  bool validTransactionId(const String& transaction_id) {
    if (transaction_id.length() == 0 || transaction_id.length() > 40)
      return false;

    for (size_t i = 0; i < transaction_id.length(); i++) {
      char c = transaction_id.charAt(i);
      if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.'))
        return false;
    }
    return true;
  }

  void machineResult(
    const String& transaction_id,
    const char* command,
    const char* status,
    const char* code,
    size_t files = 0,
    size_t bytes = 0,
    bool rebooting = false
  ) {
    Serial.printf(
      "@MARAUDER:{\"protocol\":1,\"tx\":\"%s\",\"command\":\"%s\","
      "\"status\":\"%s\",\"code\":\"%s\",\"files\":%u,\"bytes\":%u,"
      "\"rebooting\":%s}\n",
      transaction_id.c_str(), command, status, code,
      (unsigned)files, (unsigned)bytes, rebooting ? "true" : "false"
    );
  }

  const char* storageErrorCode(uint8_t error, const char* fallback) {
    if (error == 1)
      return "SD_NOT_READY";
    if (error == 2)
      return "BACKUP_NOT_FOUND";
    return fallback;
  }
}
// GCOVR_EXCL_STOP

// Brightness functions defined in esp32_marauder.ino
#ifndef HAS_MINI_SCREEN
  extern void brightnessCycle();
  extern uint8_t getBrightnessLevel();
#endif

void CommandLine::RunSetup() {
  #ifndef MARAUDER_V8
    Serial.println(this->ascii_art);
  #endif

  Serial.println(F("\n\n--------------------------------\n"));
  Serial.println(F("         ESP32 Marauder      \n"));
  Serial.println("            " + version_number + "\n");
  Serial.println(F("       By: justcallmekoko\n"));
  Serial.println(F("--------------------------------\n\n"));
  
  Serial.print("> ");
}

String CommandLine::getSerialInput() {
  String input = "";

  if (Serial.available() > 0)
    input = Serial.readStringUntil('\n');

  input.trim();
  return input;
}

void CommandLine::main(uint32_t currentTime) {
  String input = this->getSerialInput();

  this->runCommand(input);

  if (input != "")
    Serial.print("> ");
}

LinkedList<String> CommandLine::parseCommand(String input, char* delim) {
  LinkedList<String> cmd_args;

  bool inQuote = false;
  bool inApostrophe = false;
  String buffer = "";

  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);

    if (c == '"') {
      // Check if the quote is within an apostrophe
      if (inApostrophe) {
        buffer += c;
      } else {
        inQuote = !inQuote;
      }
    } else if (c == '\'') {
      // Check if the apostrophe is within a quote
      if (inQuote) {
        buffer += c;
      } else {
        inApostrophe = !inApostrophe;
      }
    } else if (!inQuote && !inApostrophe && strchr(delim, c) != NULL) {
      cmd_args.add(buffer);
      buffer = "";
    } else {
      buffer += c;
    }
  }

  // Add the last argument
  if (!buffer.isEmpty()) {
    cmd_args.add(buffer);
  }

  return cmd_args;
}

int CommandLine::argSearch(LinkedList<String>* cmd_args_list, const char* key) {
  if (!cmd_args_list || !key)
    return -1;

  for (int i = 0; i < cmd_args_list->size(); i++) {
    if (strcmp(cmd_args_list->get(i).c_str(), key) == 0)
      return i;
  }

  return -1;
}

bool CommandLine::checkValueExists(LinkedList<String>* cmd_args_list, int index) {
  if (index < cmd_args_list->size() - 1)
    return true;
    
  return false;
}

bool CommandLine::inRange(int max, int index) {
  if ((index >= 0) && (index < max))
    return true;

  return false;
}

/*bool CommandLine::apSelected() {
  for (int i = 0; i < access_points->size(); i++) {
    if (access_points->get(i).selected)
      return true;
  }

  return false;
}*/

bool CommandLine::hasSSIDs() {
  if (ssids->size() == 0)
    return false;

  return true;
}

void CommandLine::showCounts(int selected, int unselected) {
  Serial.print((String) selected + " selected");
  
  if (unselected != -1) 
    Serial.print(", " + (String) unselected + " unselected");
  
  Serial.println("");
}

String CommandLine::toLowerCase(String str) {
  String result = str;
  for (int i = 0; i < str.length(); i++) {
    int charValue = str.charAt(i);
    if (charValue >= 65 && charValue <= 90) { // ASCII codes for uppercase letters
      charValue += 32;
      result.setCharAt(i, char(charValue));
    }
  }
  return result;
}

void CommandLine::filterAccessPoints(String filter) {
  int count_selected = 0;
  int count_unselected = 0;

  // Split the filter string into individual filters
  LinkedList<String> filters;
  int start = 0;
  int end = filter.indexOf(" or ");
  while (end != -1) {
    filters.add(filter.substring(start, end));
    start = end + 4;
    end = filter.indexOf(" or ", start);
  }
  filters.add(filter.substring(start));

  // Loop over each access point and check if it matches any of the filters
  for (int i = 0; i < access_points->size(); i++) {
    AccessPoint access_point = access_points->get(i);
    bool matchesFilter = false;
    for (int j = 0; j < filters.size(); j++) {
      String f = toLowerCase(filters.get(j));
      if (f.substring(0, 7) == "equals ") {
        String ssidEquals = f.substring(7);
        if ((ssidEquals.charAt(0) == '\"' && ssidEquals.charAt(ssidEquals.length() - 1) == '\"' && ssidEquals.length() > 1) ||
            (ssidEquals.charAt(0) == '\'' && ssidEquals.charAt(ssidEquals.length() - 1) == '\'' && ssidEquals.length() > 1)) {
          ssidEquals = ssidEquals.substring(1, ssidEquals.length() - 1);
        }
        if (access_point.essid.equalsIgnoreCase(ssidEquals)) {
          matchesFilter = true;
          break;
        }
      } else if (f.substring(0, 9) == "contains ") {
        String ssidContains = f.substring(9);
        if ((ssidContains.charAt(0) == '\"' && ssidContains.charAt(ssidContains.length() - 1) == '\"' && ssidContains.length() > 1) ||
            (ssidContains.charAt(0) == '\'' && ssidContains.charAt(ssidContains.length() - 1) == '\'' && ssidContains.length() > 1)) {
          ssidContains = ssidContains.substring(1, ssidContains.length() - 1);
        }
        String essid = toLowerCase(access_point.essid);
        if (essid.indexOf(ssidContains) != -1) {
          matchesFilter = true;
          break;
        }
      }
    }
    // Toggles the selected state of the AP
    access_point.selected = matchesFilter;
    access_points->set(i, access_point);

    if (matchesFilter) {
      count_selected++;
    } else {
      count_unselected++;
    }
  }

  this->showCounts(count_selected, count_unselected);
}

void CommandLine::startScanFromCLI(int scan_mode, uint16_t color, const char* scan_name) {
  // Pure Wardrive build: same allowlist as WiFiScan::StartScan so blocked
  // modes get clean CLI feedback instead of a bare "Starting..." line.
  switch (scan_mode) {
    case WIFI_SCAN_OFF:
    case LV_JOIN_WIFI:
    case LV_ADD_SSID:
    case LV_SELECT_AP:
    case WIFI_CONNECTED:
    case WIFI_HOSTSPOT:
    case OTA_UPDATE:
    case SHOW_INFO:
    case ESP_UPDATE:
    case WIFI_SCAN_GPS_DATA:
    case WIFI_SCAN_GPS_NMEA:
    case GPS_TRACKER:
    case WIFI_SCAN_WAR_DRIVE:
    case WIFI_SCAN_STATION_WAR_DRIVE:
    case BT_SCAN_WAR_DRIVE:
    case BT_SCAN_WAR_DRIVE_CONT:
      break;
    default:
      Serial.println(F("Disabled in Pure Wardrive build"));
      return;
  }
  Serial.print(F("Starting"));
  Serial.print(scan_name);
  Serial.print(F(". Stop with "));
  Serial.println(STOPSCAN_CMD);
  #ifdef HAS_SCREEN
    display_obj.clearScreen();
    menu_function_obj.drawStatusBar();
  #endif
  wifi_scan_obj.StartScan(scan_mode, color);
}

void CommandLine::runCommand(String input) {
  if (input == "") return;

  if(wifi_scan_obj.scanning() && wifi_scan_obj.currentScanMode == WIFI_SCAN_GPS_NMEA){
    if(input != STOPSCAN_CMD) return;    
  }
  else
    Serial.println("#" + input);

  LinkedList<String> cmd_args = this->parseCommand(input, " ");
  
  //// Admin commands
  // Help
  if (cmd_args.get(0) == HELP_CMD) {
    Serial.println(HELP_HEAD);
    Serial.println(HELP_CH_CMD);
    Serial.println(HELP_SETTINGS_CMD);
    Serial.println(HELP_GEOFENCE_CMD);
    Serial.println(HELP_REBOOT_CMD);
    Serial.println(HELP_UPDATE_CMD_A);
    Serial.println(HELP_LS_CMD);
    // GCOVR_EXCL_START -- hardware-only command help entry.
    Serial.println(HELP_PROTOCOL_INFO_CMD);
    #ifdef HAS_SD
      Serial.println(HELP_BACKUP_SPIFFS_CMD);
      Serial.println(HELP_BACKUP_STATUS_CMD);
      Serial.println(HELP_RESTORE_SPIFFS_CMD);
    #endif
    // GCOVR_EXCL_STOP
    Serial.println(HELP_LED_CMD);
    Serial.println(HELP_GPS_DATA_CMD);
    Serial.println(HELP_GPS_CMD);
    Serial.println(HELP_NMEA_CMD);
    Serial.println(HELP_GPS_TRACKER_CMD);
    Serial.println(HELP_STOPSCAN_CMD);
    #ifdef HAS_GPS
      Serial.println(HELP_WARDRIVE_CMD);
    #endif
    // WiFi Aux (Pure Wardrive)
    Serial.println(HELP_JOIN_CMD);
    Serial.println(HELP_UPLOAD_CMD);

    // Bluetooth sniff/scan
    // Pure Wardrive: Bluetooth pentest commands removed.
    Serial.println(HELP_BRIGHTNESS_CMD);
    Serial.println(HELP_FOOT);
    return;
  }

  // Stop Scan
  if (cmd_args.get(0) == STOPSCAN_CMD) {
    int f_arg = this->argSearch(&cmd_args, "-f");
    
    uint8_t old_scan_mode=wifi_scan_obj.currentScanMode;

    if (f_arg != -1) {
      WiFi.disconnect(true);
      delay(100);
    }

    wifi_scan_obj.StartScan(WIFI_SCAN_OFF);
    // Pure Wardrive: ReconMission removed.

    if(old_scan_mode == WIFI_SCAN_GPS_NMEA)
      Serial.println(F("END OF NMEA STREAM"));
    else if(old_scan_mode == WIFI_SCAN_GPS_DATA)
      Serial.println(F("Stopping GPS data updates"));
    else
      Serial.println(F("Stopping WiFi tran/recv"));

    // If we don't do this, the text and button coordinates will be off
    #ifdef HAS_SCREEN
      display_obj.init();
      menu_function_obj.changeMenu(menu_function_obj.current_menu);
    #endif
  }
  else if (cmd_args.get(0) == RECON_CMD) {
    // Pure Wardrive: Recon removed.
    Serial.println(F("Disabled in Pure Wardrive build"));
  }
  else if (cmd_args.get(0) == GPS_DATA_CMD) {
    #ifdef HAS_GPS
      if (gps_obj.getGpsModuleStatus()) {
        Serial.print(F("Getting GPS Data. Stop with "));
        Serial.println((String)STOPSCAN_CMD);
        wifi_scan_obj.currentScanMode = WIFI_SCAN_GPS_DATA;
        #ifdef HAS_SCREEN
          menu_function_obj.changeMenu(&menu_function_obj.gpsInfoMenu);
        #endif
        wifi_scan_obj.StartScan(WIFI_SCAN_GPS_DATA, TFT_CYAN);
      }
    #endif
  }
  else if (cmd_args.get(0) == GPS_CMD) {
    #ifdef HAS_GPS
      if (gps_obj.getGpsModuleStatus()) {
        int get_arg = this->argSearch(&cmd_args, "-g");
        int track_arg = this->argSearch(&cmd_args, "-t");
        int nmea_arg = this->argSearch(&cmd_args, "-n");

        if (get_arg != -1) {
          String gps_info = cmd_args.get(get_arg + 1);

          if (gps_info == "fix")
            Serial.println("Fix: " + gps_obj.getFixStatusAsString());
          else if (gps_info == "sat")
            Serial.println("Sats: " + gps_obj.getNumSatsString());
          else if (gps_info == "lat")
            Serial.println("Lat: " + gps_obj.getLat());
          else if (gps_info == "lon")
            Serial.println("Lon: " + gps_obj.getLon());
          else if (gps_info == "alt")
            Serial.println("Alt: " + (String)gps_obj.getAlt());
          else if (gps_info == "accuracy")
            Serial.println("Accuracy: " + (String)gps_obj.getAccuracy());
          else if (gps_info == "date")
            Serial.println("Date/Time: " + gps_obj.getDatetime());
          else if (gps_info == "text"){
            Serial.println(gps_obj.getText());
          }
          else if (gps_info == "nmea"){
            int notparsed_arg = this->argSearch(&cmd_args, "-p");
            int notimp_arg = this->argSearch(&cmd_args, "-i");
            int recd_arg = this->argSearch(&cmd_args, "-r");
            if(notparsed_arg == -1 && notimp_arg == -1 && recd_arg == -1){
              gps_obj.sendSentence(Serial, gps_obj.generateGXgga().c_str());
              gps_obj.sendSentence(Serial, gps_obj.generateGXrmc().c_str());
            }
            else if(notparsed_arg == -1 && notimp_arg == -1)
              Serial.println(gps_obj.getNmea());
            else if(notparsed_arg == -1)
              Serial.println(gps_obj.getNmeaNotimp());
            else
              Serial.println(gps_obj.getNmeaNotparsed());
          }
          else
            Serial.println(F("You did not provide a valid argument"));
        }
        else if(nmea_arg != -1){
          String nmea_type = cmd_args.get(nmea_arg + 1);

          if (nmea_type == "native" || nmea_type == "all" || nmea_type == "gps" || nmea_type == "glonass"
              || nmea_type == "galileo" || nmea_type == "navic" || nmea_type == "qzss" || nmea_type == "beidou"){
            if(nmea_type == "beidou"){
              int beidou_bd_arg = this->argSearch(&cmd_args, "-b");
              if(beidou_bd_arg != -1)
                nmea_type="beidou_bd";
            }
            gps_obj.setType(nmea_type);
            Serial.print(F("GPS Output Type Set To: "));
            Serial.println(nmea_type);
          }
        }
        else if (track_arg != -1) {
          wifi_scan_obj.currentScanMode = GPS_TRACKER;
          #ifdef HAS_SCREEN
            menu_function_obj.changeMenu(&menu_function_obj.gpsInfoMenu);
          #endif
          wifi_scan_obj.StartScan(GPS_TRACKER, TFT_CYAN);
        }
        else if(cmd_args.size()>1)
          Serial.println(F("You did not provide a valid flag"));
      }
    #endif
  }
  else if (cmd_args.get(0) == NMEA_CMD) {
    #ifdef HAS_GPS
      if (gps_obj.getGpsModuleStatus()) {
        #ifdef HAS_SCREEN
          menu_function_obj.changeMenu(&menu_function_obj.gpsInfoMenu);
        #endif
        wifi_scan_obj.currentScanMode = WIFI_SCAN_GPS_NMEA;
        wifi_scan_obj.StartScan(WIFI_SCAN_GPS_NMEA, TFT_CYAN);
      }
    #endif
  }
  // LED command
  else if (cmd_args.get(0) == LED_CMD) {
    int hex_arg = this->argSearch(&cmd_args, "-s");
    int pat_arg = this->argSearch(&cmd_args, "-p");
    #if defined(PIN) && defined(HAS_NEOPIXEL_LED) 
      if (hex_arg != -1) {
        String hexstring = cmd_args.get(hex_arg + 1);
        int number = (int)strtol(&hexstring[1], NULL, 16);
        int r = number >> 16;
        int g = number >> 8 & 0xFF;
        int b = number & 0xFF;
        //Serial.println(r);
        //Serial.println(g);
        //Serial.println(b);
        led_obj.setColor(r, g, b);
        led_obj.setMode(MODE_CUSTOM);
      }
      else if (pat_arg != -1) {
        String pat_name = cmd_args.get(pat_arg + 1);
        pat_name.toLowerCase();
        if (pat_name == "rainbow") {
          led_obj.setMode(MODE_RAINBOW);
        }
      }
    #endif
  }
  // ls command
  else if (cmd_args.get(0) == LS_CMD) {
    #ifdef HAS_SD
      if (cmd_args.size() > 1)
        sd_obj.listDir(cmd_args.get(1));
    #endif
  }
  // GCOVR_EXCL_START -- requires mounted SPIFFS and SD filesystems.
  else if (cmd_args.get(0) == PROTOCOL_INFO_CMD) {
    int machine_arg = this->argSearch(&cmd_args, "--machine");
    String transaction_id = machine_arg >= 0 && machine_arg + 1 < cmd_args.size()
      ? cmd_args.get(machine_arg + 1) : "";
    if (machine_arg >= 0 && !validTransactionId(transaction_id))
      machineResult(transaction_id, PROTOCOL_INFO_CMD, "error", "INVALID_TRANSACTION");
    else if (machine_arg >= 0) {
      #ifdef HAS_SD
        Serial.printf(
          "@MARAUDER:{\"protocol\":1,\"tx\":\"%s\",\"command\":\"protocolinfo\","
          "\"status\":\"success\",\"code\":\"OK\",\"firmware\":\"%s\","
          "\"capabilities\":[\"spiffs-backup\",\"spiffs-backup-status\","
          "\"spiffs-restore\"],\"backupPath\":\"/spiffs\"}\n",
          transaction_id.c_str(), version_number.c_str()
        );
      #else
        Serial.printf(
          "@MARAUDER:{\"protocol\":1,\"tx\":\"%s\",\"command\":\"protocolinfo\","
          "\"status\":\"success\",\"code\":\"OK\",\"firmware\":\"%s\","
          "\"capabilities\":[]}\n",
          transaction_id.c_str(), version_number.c_str()
        );
      #endif
    }
  }
  else if (cmd_args.get(0) == BACKUP_SPIFFS_CMD ||
           cmd_args.get(0) == BACKUP_STATUS_CMD ||
           cmd_args.get(0) == RESTORE_SPIFFS_CMD) {
    uint8_t operation = cmd_args.get(0) == BACKUP_SPIFFS_CMD ? 0 :
                        cmd_args.get(0) == BACKUP_STATUS_CMD ? 1 : 2;
    const char* command = operation == 0 ? BACKUP_SPIFFS_CMD :
                          operation == 1 ? BACKUP_STATUS_CMD : RESTORE_SPIFFS_CMD;
    int machine_arg = this->argSearch(&cmd_args, "--machine");
    String transaction_id = machine_arg >= 0 && machine_arg + 1 < cmd_args.size()
      ? cmd_args.get(machine_arg + 1) : "";
    bool machine = machine_arg >= 0;
    if (machine && !validTransactionId(transaction_id)) {
      machineResult(transaction_id, command, "error", "INVALID_TRANSACTION");
      return;
    }
    #ifdef HAS_SD
      size_t files = 0;
      size_t bytes = 0;
      uint8_t error = 0;
      if (machine && operation != 1)
        machineResult(transaction_id, command, "started", "OK");
      else if (!machine && operation != 1)
        Serial.printf("SPIFFS %s started\n", operation == 0 ? "backup" : "restore");

      bool success = sd_obj.migrateSPIFFS(operation, files, bytes, error);
      if (machine) {
        if (success)
          machineResult(transaction_id, command, "success", "OK", files, bytes, operation == 2);
        else {
          const char* fallback = operation == 0 ? "BACKUP_FAILED" :
                                 operation == 1 ? "BACKUP_INSPECTION_FAILED" : "RESTORE_FAILED";
          machineResult(transaction_id, command, "error", storageErrorCode(error, fallback));
        }
      }
      else if (success) {
        const char* action = operation == 0 ? "backup complete" :
                             operation == 1 ? "backup status" : "restore complete";
        Serial.printf("SPIFFS %s: %u files, %u bytes%s\n", action,
                      static_cast<unsigned int>(files),
                      static_cast<unsigned int>(bytes),
                      operation == 2 ? "; rebooting" : "");
      }
      else {
        const char* fallback = operation == 0 ? "BACKUP_FAILED" :
                               operation == 1 ? "BACKUP_INSPECTION_FAILED" : "RESTORE_FAILED";
        Serial.printf("SPIFFS %s failed: %s\n",
                      operation == 0 ? "backup" : operation == 1 ? "backup status" : "restore",
                      storageErrorCode(error, fallback));
      }
      if (success && operation == 2) {
        delay(1000);
        ESP.restart();
      }
    #else
      if (machine)
        machineResult(transaction_id, command, "error", "SD_NOT_SUPPORTED");
      else
        Serial.println(F("SD Card NOT Supported"));
    #endif
  }
  // GCOVR_EXCL_STOP

  // Channel command
  else if (cmd_args.get(0) == CH_CMD) {
    // Search for channel set arg
    int ch_set = this->argSearch(&cmd_args, "-s");

    if (ch_set != -1) {
      wifi_scan_obj.set_channel = cmd_args.get(ch_set + 1).toInt();
      wifi_scan_obj.changeChannel();
      Serial.println(wifi_scan_obj.set_channel);
    }
    Serial.println(wifi_scan_obj.set_channel);
  }
  // Pure Wardrive: clearlist command removed.

  else if (cmd_args.get(0) == UPLOAD_CMD) {
    #ifdef HAS_DIRECT_UPLOAD
      int dest_sw = this->argSearch(&cmd_args, "-d");
      String upload_dest_arg = cmd_args.get(dest_sw + 1);
      int upload_dest = -1;

      if (upload_dest_arg == "wdg")
        upload_dest = WDG_UPLOAD;
      else if (upload_dest_arg == "wigle")
        upload_dest = WIGLE_UPLOAD;
      else if (upload_dest_arg == "both")
        upload_dest = BOTH_UPLOAD;

      if (upload_dest > -1) {
        if (!wifi_scan_obj.joinSavedWiFi(false)) {
          Serial.println(F("Failed to connect to saved WiFi"));
          return;
        }
        delay(1000);
        for (int i = 0; i < sd_obj.sd_files->size(); i++) {
          if (sd_obj.sd_files->get(i).startsWith("wardrive_") || sd_obj.sd_files->get(i).startsWith("wigle-")) {
            if (!sd_obj.sd_files->get(i).endsWith(".wigle") && !sd_obj.sd_files->get(i).endsWith(".wdg") && !sd_obj.sd_files->get(i).endsWith(".gpx")) {
              Serial.println("Uploading " + sd_obj.sd_files->get(i) + "...");
              if (wifi_scan_obj.uploadFile("/" + sd_obj.sd_files->get(i), true, upload_dest)) {
                Serial.println("Upload OK");
              } else {
                Serial.println("WiGLE failed");
              }
            }
          }
        }
        WiFi.disconnect(true);
        delay(100);
        wifi_scan_obj.StartScan(WIFI_SCAN_OFF, TFT_RED);

        Serial.println("Upload complete");
      }
    #else
      Serial.println("Direct upload not supported");
    #endif
  }

  else if (cmd_args.get(0) == SETTINGS_CMD) {
    int ss_sw = this->argSearch(&cmd_args, "-s"); // Set setting
    int re_sw = this->argSearch(&cmd_args, "-r"); // Reset setting
    int en_sw = this->argSearch(&cmd_args, "enable"); // enable setting
    int da_sw = this->argSearch(&cmd_args, "disable"); // disable setting

    if (re_sw != -1) {
      settings_obj.createDefaultSettings(SPIFFS);
      return;
    }

    if (ss_sw == -1) {
      settings_obj.printJsonSettings(settings_obj.getSettingsString());
    }
    else {
      bool result = false;
      String setting_name = cmd_args.get(ss_sw + 1);
      if (en_sw != -1)
        result = settings_obj.saveSetting<bool>(setting_name.c_str(), true);
      else if (da_sw != -1)
        result = settings_obj.saveSetting<bool>(setting_name.c_str(), false);
      else
        return;

      if (!result) {
        Serial.print(F("Could not successfully update setting \""));
        Serial.println(setting_name + "\"");
        return;
      }
    }
  }

  else if (cmd_args.get(0) == REBOOT_CMD)
    ESP.restart();

  //// WiFi/Bluetooth Scan/Attack commands
  if (!wifi_scan_obj.scanning()) {
    // Dump pcap/log to serial too, valid for all scan/attack commands
    wifi_scan_obj.save_serial = this->argSearch(&cmd_args, "-serial") != -1;

    // Geofence configuration
    if (cmd_args.get(0) == GEOFENCE_CMD) {
      if (cmd_args.size() == 2 && cmd_args.get(1) == "list") {
        bool any = false;
        for (uint8_t i = 0; i < MAX_GEOFENCES; i++) {
          GeofenceConfig fence;
          if (!settings_obj.loadGeofence(i, fence)) continue;
          any = true;
          Serial.printf("%u: %s | %.6f, %.6f | %.2f mi\n", i + 1, fence.name.c_str(), fence.latitude, fence.longitude, fence.radiusMiles);
        }
        if (!any) Serial.println(F("No geofences configured"));
      }
      else if (cmd_args.size() == 3 && cmd_args.get(1) == "clear") {
        const int slot = cmd_args.get(2).toInt();
        if (slot < 1 || slot > MAX_GEOFENCES) Serial.println(F("Invalid slot; use 1-5"));
        else {
          settings_obj.clearGeofence(slot - 1);
          wifi_scan_obj.reloadGeofences();
          Serial.printf("Geofence %d cleared\n", slot);
        }
      }
      else if (cmd_args.size() == 7 && cmd_args.get(1) == "set") {
        const int slot = cmd_args.get(2).toInt();
        double lat, lon, radius;
        GeofenceConfig fence;
        fence.enabled = true;
        fence.name = cmd_args.get(6);
        if (slot < 1 || slot > MAX_GEOFENCES || !parseFiniteNumber(cmd_args.get(3), lat) ||
            !parseFiniteNumber(cmd_args.get(4), lon) || !parseFiniteNumber(cmd_args.get(5), radius)) {
          Serial.println(HELP_GEOFENCE_CMD);
        } else {
          fence.latitude = lat;
          fence.longitude = lon;
          fence.radiusMiles = radius;
          if (!settings_obj.saveGeofence(slot - 1, fence)) Serial.println(F("Invalid geofence: check name, coordinates, and radius"));
          else {
            wifi_scan_obj.reloadGeofences();
            Serial.printf("Geofence %d saved\n", slot);
          }
        }
      }
      else Serial.println(HELP_GEOFENCE_CMD);
    }
    // Wardrive
    else if (cmd_args.get(0) == WARDRIVE_CMD) {
      #ifdef HAS_GPS
        if (gps_obj.getGpsModuleStatus()) {
          //int sta_sw = this->argSearch(&cmd_args, "-s");
          this->startScanFromCLI(WIFI_SCAN_WAR_DRIVE, TFT_GREEN, "Wardrive");
        }
      #else
        Serial.println(F("GPS not supported"));
      #endif
    }
    // Pure Wardrive: pentest commands removed.

    // Pure Wardrive: pentest commands removed.

    // Brightness command
    else if (cmd_args.get(0) == BRIGHTNESS_CMD) {
      #ifndef HAS_MINI_SCREEN
        int c_arg = this->argSearch(&cmd_args, "-c");
        int s_arg = this->argSearch(&cmd_args, "-s");
        if (c_arg != -1) {
          brightnessCycle();
        } else if (s_arg != -1) {
          uint8_t lvl = cmd_args.get(s_arg + 1).toInt();
          if (lvl < 10) {
            extern void brightnessSave(uint8_t level);
            brightnessSave(lvl);
            Serial.print(F("[Brightness] Set to level "));
            Serial.println(lvl);
          } else {
            Serial.println(F("Level must be 0-9"));
          }
        } else {
          Serial.print(F("[Brightness] Current level: "));
          Serial.println(getBrightnessLevel());
        }
      #endif
    }

    // Wardrive POI removed in Pure Wardrive build (Geofences only)
    else if (cmd_args.get(0) == WARDRIVEPOI_CMD) {
      Serial.println(F("Disabled in Pure Wardrive build"));
    }

    // Update command
    if (cmd_args.get(0) == UPDATE_CMD) {
      int sd_sw = this->argSearch(&cmd_args, "-s"); // SD Update
      if (sd_sw != -1) {
        #ifdef HAS_SD
          if (!sd_obj.supported) {
            Serial.println(F("SD card is not connected."));
            return;
          }
          wifi_scan_obj.currentScanMode = OTA_UPDATE;
          sd_obj.runUpdate();
        #endif
      }
    }
  }

  if (wifi_scan_obj.wifi_connected) {
    // Pure Wardrive: pentest commands removed.

    // GPS POI removed in Pure Wardrive build (Geofences only)
    if (cmd_args.get(0) == GPS_POI_CMD) {
      Serial.println(F("Disabled in Pure Wardrive build"));
    }

    // Pure Wardrive: pentest commands removed.
  }


  int count_selected = 0;
  //// WiFi aux commands (Pure Wardrive: list/info/select removed)
  if (cmd_args.get(0) == JOIN_CMD) {
    int ap_sw = this->argSearch(&cmd_args, "-a");
    int pw_sw = this->argSearch(&cmd_args, "-p");
    int s_sw  = this->argSearch(&cmd_args, "-s");

    if ((ap_sw != -1) && (pw_sw != -1)) {
      int index = cmd_args.get(ap_sw + 1).toInt();
      String password = cmd_args.get(pw_sw + 1);
      AccessPoint access_point = access_points->get(index);
      Serial.println("Using SSID: " + (String)access_point.essid);
      //wifi_scan_obj.currentScanMode = LV_JOIN_WIFI;
      //wifi_scan_obj.StartScan(LV_JOIN_WIFI, TFT_YELLOW); 
      wifi_scan_obj.joinWiFi(access_point.essid, password, false);
      #ifdef HAS_SCREEN
        #ifdef HAS_MINI_KB
          menu_function_obj.changeMenu(menu_function_obj.current_menu);
        #endif
      #endif
    }
    else if (s_sw != -1) {
      if (settings_obj.getSavedWifiCount() > 0) {
        wifi_scan_obj.joinSavedWiFi(false);
        #ifdef HAS_SCREEN
          menu_function_obj.changeMenu(menu_function_obj.current_menu);
        #endif
      }
      else {
        Serial.println(F("There are no saved WiFi credentials"));
      }
    }
    else {
      Serial.println(F("You did not provide the proper args"));
      return;
    }
  }
  // Pure Wardrive: select command removed.
  // Pure Wardrive: save/load/add/ssid commands removed.
}

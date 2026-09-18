#pragma once

#ifndef MenuFunctions_h
#define MenuFunctions_h

#include "configs.h"

#if defined(MARAUDER_CARDPUTER) || defined(MARAUDER_CARDPUTER_ADV)
  #include "Keyboard.h"
#endif

#ifdef HAS_TOUCH
  #include "TouchKeyboard.h"
#endif

#ifdef HAS_SCREEN

#define BATTERY_ANALOG_ON 0

#include "WiFiScan.h"
// Pure Wardrive: ReconMission removed.
#include "TargetListSort.h"
#include "BatteryInterface.h"
#include "SDInterface.h"
#include "settings.h"
#include "MenuInputRepeat.h"

#ifdef HAS_BUTTONS
  #include "Switches.h"
  #if (U_BTN >= 0)
    extern Switches u_btn;
  #endif
  #if (D_BTN >= 0)
    extern Switches d_btn;
  #endif
  #if (L_BTN >= 0)
    extern Switches l_btn;
  #endif
  #if (R_BTN >= 0)
    extern Switches r_btn;
  #endif
  #if (C_BTN >= 0)
    extern Switches c_btn;
  #endif
#endif

extern WiFiScan wifi_scan_obj;
// Pure Wardrive: ReconMission removed.
extern SDInterface sd_obj;
// #ifdef HAS_BATTERY
extern BatteryInterface battery_obj;
// #endif
extern Settings settings_obj;

#define FLASH_BUTTON 0

#if BATTERY_ANALOG_ON == 1
#define BATTERY_PIN 13
#define ANALOG_PIN 34
#define CHARGING_PIN 27
#endif

// Icon definitions
#define ATTACKS 0
#define BEACON_SNIFF 1
#define BLUETOOTH 2
#define BLUETOOTH_SNIFF 3
#define DEAUTH_SNIFF 4
#define DRAW 5
#define PACKET_MONITOR 6
#define PROBE_SNIFF 7
#define SCANNERS 8
#define CC_SKIMMERS 9
#define SNIFFERS 10
#define WIFI 11
#define BEACON_SPAM 12
#define RICK_ROLL 13
#define REBOOT 14
#define GENERAL_APPS 15
#define UPDATE 16
#define DEVICE 17
#define DEVICE_INFO 18
#define SD_UPDATE 19
#define WEB_UPDATE 20
#define EAPOL 21
#define STATUS_BAT 22
#define STATUS_SD 23
#define PWNAGOTCHI 24
#define SHUTDOWN 25
#define BEACON_LIST 26
#define GENERATE 27
#define CLEAR_ICO 28
#define KEYBOARD_ICO 29
#define JOIN_WIFI 30
#define LANGUAGE 31
#define STATUS_GPS 32
#define GPS_MENU 33
#define DISABLE_TOUCH 34
#define FLIPPER 35
#define BLANK 36
#define PINESCAN_SNIFF 37 // Use blanks icon
#define MULTISSID_SNIFF 37 // Use blanks icon
#define JOINED 38
#define FORCE 39
#define FUNNY_BEACON 40
#define FLOCK 41
#define BRIGHTNESS 42
#define SETTINGS 43

struct Menu;

// Individual Nodes of a menu

struct MenuNode {
  String name;
  bool command;
  uint8_t color;
  uint8_t icon;
  bool selected;
  std::function<void()> callable;
};

// Full Menus
struct Menu {
  String name;
  LinkedList<MenuNode>* list;
  Menu                * parentMenu;
  uint16_t               selected = 0;
};


class MenuFunctions
{
  private:

    // Pure Wardrive: FoxHunt removed.

    String u_result = "";


    float _graph_scale = 1.0;
    uint32_t initTime = 0;
    uint32_t last_scan_toggle_ms = 0;
    int menu_start_index = 0;
    uint8_t mini_kb_index = 0;
    uint8_t old_gps_sat_count = 0;
    uint8_t max_graph_value = 0;
    Menu* marquee_menu = nullptr;
    uint16_t marquee_selected = 0xFFFF;
    uint16_t marquee_rendered_offset = 0;
    uint16_t marquee_max_offset = 0;
    uint32_t marquee_selected_since = 0;
    MenuInputRepeat menu_up_repeat;
    MenuInputRepeat menu_down_repeat;
    int8_t menu_touch_button = -1;

    // Pure Wardrive: FoxHunt removed.

    // Main menu stuff
    Menu mainMenu;
    Menu fullMenu;

    #ifdef HAS_GPS
      Menu gpsMenu;   // H4W9 Added GPS Menu option to Main Menu
    #endif
    Menu deviceMenu;

    // Device menu stuff
    //Menu whichUpdateMenu;
    Menu failedUpdateMenu;
    Menu updateMenu;
    Menu settingsMenu;
    Menu specSettingMenu;
    Menu geofenceMenu;
    Menu geofenceActionMenu;
    #ifdef HAS_MINI_SCREEN
      Menu geofenceRadiusMenu;
      GeofenceConfig pendingGeofence;
      uint8_t pendingGeofenceSlot = 0;
      bool geofenceMenuRefreshPending = false;
    #endif
    uint8_t selectedGeofence = 0;
    //Menu languageMenu;
    Menu sdDeleteMenu;
    LinkedList<SDDirectoryEntry>* sd_browser_entries = nullptr;
    LinkedList<String>* sd_delete_selection = nullptr;
    String sd_browser_path = "/";
    bool sd_browser_release_pending = false;
    bool saved_wifi_release_pending = false;
    void ensureSDDeleteBrowserResources();
    void releaseSDDeleteBrowserResources();

    // Pure Wardrive: pentest menus removed.
    Menu savedWifiMenu;
    Menu miniKbMenu;

    #ifdef HAS_DIRECT_UPLOAD
      Menu fileActionMenu;
    #endif

    //static void lv_tick_handler();

    // Menu icons

    void buildUploadFileMenu();
    void buildHomeMenu();
    void displayHomeMenu();
    void buildFileActionMenu(const String& filename);
    void doUpload(const String& filename, uint8_t uploadType, const char* label);
    void setupSDFileList(bool update = false);
    void buildSDFileMenu(bool update = false);
    void buildSavedWifiMenu(bool replace_mode = false);
    void buildGeofenceMenu();
    void buildGeofenceActionMenu(uint8_t slot);
    String geofenceTextInput(const char* title);
    bool editGeofence(uint8_t slot, bool use_current_location);
    #ifdef HAS_MINI_SCREEN
      void beginMiniGeofenceEdit(uint8_t slot, bool use_current_location);
      void buildGeofenceRadiusMenu(uint8_t slot, const GeofenceConfig& fence);
      void deferGeofenceMenuRefresh();
    #endif
    void releaseSavedWifiMenu();
    void buildSDDeleteBrowser(const String& path, bool reset_selection = false);
    void toggleSDDeleteSelection(const String& path);
    bool isSDFileSelected(const String& path) const;
    String parentSDPath(const String& path) const;
    void displayMenuButtons();
    uint16_t getColor(uint16_t color);
    void drawAvgLine(int16_t value);
    void drawMaxLine(int16_t value, uint16_t color);
    void drawMaxLine(uint8_t value, uint16_t color);
    float calculateGraphScale(int16_t value);
    float calculateGraphScale(uint8_t value);
    float graphScaleCheck(const int16_t array[TFT_WIDTH]);
    #ifndef HAS_DUAL_BAND
      float graphScaleCheckSmall(const uint8_t array[MAX_CHANNEL]);
    #else
      float graphScaleCheckSmall(const uint8_t array[DUAL_BAND_CHANNELS]);
    #endif
    void drawGraph(int16_t *values);
    void drawGraphSmall(uint8_t *values);
    void renderGraphUI(uint8_t scan_mode = 0);
    void addNodes(Menu* menu, const char* name, uint8_t color, int place, std::function<void()> callable, bool selected = false);
    void battery(bool initial = false);
    void battery2(bool initial = false);
    const char* callSetting(const char* key);
    void displaySetting(const char* key, Menu* menu, int index);
    void buttonSelected(int b, int x = -1, uint16_t text_offset = 0);
    void buttonNotSelected(int b, int x = -1);
    String menuLabelWindow(const String& name, uint16_t offset = 0);
    uint16_t menuLabelMaxOffset(const String& name);
    void updateMenuMarquee(uint32_t current_time);
    #ifdef HAS_MINI_SCREEN
      void drawMiniMenuButton(int b, int x, bool selected, uint16_t text_offset = 0);
    #endif
    //#if (!defined(HAS_ILI9341) && defined(HAS_BUTTONS))
    #ifdef HAS_MINI_KB
      String miniKeyboard(Menu * targetMenu, bool do_pass = false);
    #endif
    //#endif

    #if defined(MARAUDER_CARDPUTER) || defined(MARAUDER_CARDPUTER_ADV)
      Keyboard_Class M5CardputerKeyboard = Keyboard_Class();
      void updateKeyboard();
      bool isKeyPressed(char c);
    #endif

  public:
    Menu* current_menu;

    #ifdef HAS_GPS
      // GPS Menu
      Menu gpsInfoMenu;
    #endif

    Menu infoMenu;

    #ifdef HAS_DIRECT_UPLOAD
      Menu uploadLogsMenu;
    #endif

    //Ticker tick;

    uint16_t x = -1, y = -1;
    boolean pressed = false;

    bool disable_touch;

    String loaded_file = "";

    void setGraphScale(float scale);
    void updateStatusBar();
    void buildButtons(Menu* menu, int starting_index = 0, const char* button_name = nullptr);
    void changeMenu(Menu* menu, bool simple_change = false);
    void drawStatusBar();
    void displayCurrentMenu(int start_index = 0);
    #ifndef HAS_MINI_SCREEN
      void brightnessMode();
    #endif
    void main(uint32_t currentTime);
    void RunSetup();
    void orientDisplay();
    void showBootChecklist();
    // PURE WARDRIVER: true while home is shown during a running wardrive
    // (taps toggle stats/home, only STOP ends the session).
    bool show_home_during_scan = false;
};


#endif
#endif



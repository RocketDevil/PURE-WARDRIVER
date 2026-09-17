# PURE WARDRIVER

<p align="center"><img alt="PURE WARDRIVER logo" src="pictures/pure-wardriver-logo.png" width="300"></p>
<p align="center">
  <b>Pure wardriving firmware for the ESP32 Marauder V8 — no pentest tools, just wardriving.</b>
</p>

## What it is

PURE WARDRIVER turns a Marauder V8 into a dedicated wardriving device:

- **SCAN** — WiFi + BLE wardriving with GPS logging (Wigle-compatible CSV)
- **SYNC** — upload log files to [WDGWars](https://wdgwars.pl), [WiGLE](https://wigle.net), or both
- **MENU** — upload file browser, Saved WiFi, geofences, GPS tools, settings
- Boot self-test, blue touch UI, 5s SCAN toggle guard + pocket-press guard

## Hardware

Primary target: **Marauder V8** (ESP32-C5, touch display, GPS, SD).
Other boards are planned — see `esp32_marauder/configs.h` for supported targets.

## Flashing

Use a release `.bin` (or build it yourself, see below) and flash it with
`C5_Py_Flasher_for_v8/c5_flasher.py` or `esptool.py`
(ESP32-C5, 8 MB flash, `default_8MB` partition scheme).

## Building (V8)

```powershell
.\tools\arduino-cli.exe compile `
  --fqbn "esp32:esp32:esp32c5:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=enabled" `
  --build-property "compiler.cpp.extra_flags=-DMARAUDER_V8" `
  --warnings none ./esp32_marauder --output-dir ./build_pure_wardrive_v8
```

Requires Arduino-ESP32 core 3.3.4 and the libraries in your sketchbook
(see `.github/workflows/build_parallel.yml` for versions).

## First run

1. Put two files on the microSD card (root directory):
   `wifi-upload-credentials.txt`:
   ```
   ssid=YourWiFiName
   pass=YourWiFiPassword
   ```
   API keys, either as `API.txt`:
   ```
   wdg_key=YOUR_WDGWARS_KEY
   wu=YOUR_WIGLE_USERNAME
   wt=YOUR_WIGLE_TOKEN
   ```
   or as three single-value files (same effect):
   `wdg_key.txt`, `wigle_api_name.txt`, `wigle_api_token.txt`
   (each file holds just the key, nothing else).
2. Power on: the firmware loads the API keys and connects to your WiFi
   right away to verify the link. If no upload starts within 5 minutes,
   WiFi switches off again to save battery — opening SYNC reconnects.
3. Wait for GPS fix.
4. Press **SCAN** to start logging (`/wardrive_N.log` on SD).
5. Press **STOP** (5s guard against double-taps) to stop.
6. Press **SYNC**, pick a log file, choose WDGWars / WiGLE / both.

## Attribution / License

Based on [ESP32Marauder](https://github.com/justcallmekoko/ESP32Marauder)
by Just Call Me Koko, MIT licensed. This project keeps the original
`LICENSE` (MIT) and copyright notice. All pentest/attack modules were
removed; wardrive, GPS, SD, display and upload code paths are retained
from upstream.

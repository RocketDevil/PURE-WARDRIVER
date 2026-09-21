// PURE WARDRIVER CP-ADV display probe (throwaway debug, not shipped).
// Same toolchain/env as the firmware. Expected: red/green/blue flashes,
// then "PROBE OK" on black. Report what the screen shows + USB log.
#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup() {
  USBSerial.begin(115200);
  delay(500);
  USBSerial.println(F("[PROBE] init..."));
  tft.init();
  tft.setRotation(1);
  USBSerial.println(F("[PROBE] red"));
  tft.fillScreen(TFT_RED);
  delay(800);
  USBSerial.println(F("[PROBE] green"));
  tft.fillScreen(TFT_GREEN);
  delay(800);
  USBSerial.println(F("[PROBE] blue"));
  tft.fillScreen(TFT_BLUE);
  delay(800);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("PROBE OK", 120, 60, 4);
  USBSerial.println(F("[PROBE] done"));
}

void loop() {
  delay(1000);
}

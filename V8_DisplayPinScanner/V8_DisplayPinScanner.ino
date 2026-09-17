// V8 Display Pin Scanner (PURE WARDRIVER tools)
// Finds the ILI9341 wiring on Marauder V8 (ESP32-C5) without the private
// TFT setup: it bit-bangs SPI with candidate pins and reads the display
// ID register (RDID, cmd 0x04). A genuine ILI9341 answers 00 93 41 xx.
//
// Wiring assumption (to verify): SD and TFT share one SPI bus, so SCLK,
// MOSI and MISO are most likely the pancake values below. CS/DC/RST are
// scanned over all free C5 GPIOs. Backlight is probed interactively.
//
// Usage: flash with the same FQBN as the firmware, open serial monitor
// at 115200 and follow the prompts.

#include <Arduino.h>
#include <SPI.h>

// --- Stage A: assumed shared bus (pancake values). Set to -1 to scan bus too.
#define BUS_SCLK 23
#define BUS_MOSI 24
#define BUS_MISO 4

// C5 GPIO candidates for CS / DC / RST (strapping + USB pins excluded).
static const int8_t kCandidates[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                     11, 12, 13, 14, 15, 22, 23, 24, 25,
                                     26, 27, 28};
static const int kNumCandidates = sizeof(kCandidates) / sizeof(kCandidates[0]);

static const int8_t kBusSclkOpts[] = {BUS_SCLK};
static const int8_t kBusMosiOpts[] = {BUS_MOSI};
static const int8_t kBusMisoOpts[] = {BUS_MISO};

static SPIClass scanSPI(SPI);

uint32_t readDisplayId(int8_t sclk, int8_t miso, int8_t mosi, int8_t cs,
                       int8_t dc, int8_t rst) {
  scanSPI.begin(sclk, miso, mosi, cs);
  scanSPI.setFrequency(1000000);
  scanSPI.setDataMode(SPI_MODE0);
  scanSPI.setBitOrder(MSBFIRST);

  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
  pinMode(dc, OUTPUT);
  if (rst >= 0) {
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH);
    delay(10);
    digitalWrite(rst, LOW);
    delay(20);
    digitalWrite(rst, HIGH);
    delay(150);
  } else {
    delay(150);
  }

  // Software reset + sleep out (best effort, no MISO needed).
  digitalWrite(cs, LOW);
  digitalWrite(dc, LOW);
  scanSPI.transfer(0x01);  // SWRESET
  digitalWrite(cs, HIGH);
  delay(150);
  digitalWrite(cs, LOW);
  digitalWrite(dc, LOW);
  scanSPI.transfer(0x11);  // SLPOUT
  digitalWrite(cs, HIGH);
  delay(150);

  // RDID: command with DC low, then 4 dummy/read bytes with DC high.
  digitalWrite(cs, LOW);
  digitalWrite(dc, LOW);
  scanSPI.transfer(0x04);
  digitalWrite(dc, HIGH);
  uint8_t b0 = scanSPI.transfer(0x00);
  uint8_t b1 = scanSPI.transfer(0x00);
  uint8_t b2 = scanSPI.transfer(0x00);
  uint8_t b3 = scanSPI.transfer(0x00);
  digitalWrite(cs, HIGH);

  scanSPI.end();
  return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
         ((uint32_t)b2 << 8) | (uint32_t)b3;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println(F("\n[V8SCAN] Display pin scanner. ILI9341 RDID should read xx9341xx."));
  Serial.println(F("[V8SCAN] Scanning CS/DC/RST combos..."));

  const int8_t rstOpts[] = {-1, 2, 3, 5, 10, 12, 26, 27};
  const int kNumRst = sizeof(rstOpts) / sizeof(rstOpts[0]);
  uint32_t tested = 0;

  for (int ci = 0; ci < kNumCandidates; ci++) {
    for (int di = 0; di < kNumCandidates; di++) {
      if (di == ci) continue;
      for (int ri = 0; ri < kNumRst; ri++) {
        int8_t cs = kCandidates[ci];
        int8_t dc = kCandidates[di];
        int8_t rst = rstOpts[ri];
        if (rst == cs || rst == dc) continue;
        uint32_t id = readDisplayId(kBusSclkOpts[0], kBusMisoOpts[0],
                                    kBusMosiOpts[0], cs, dc, rst);
        tested++;
        uint8_t b1 = (id >> 16) & 0xFF;
        uint8_t b2 = (id >> 8) & 0xFF;
        if (b1 == 0x93 && b2 == 0x41) {
          Serial.printf("[V8SCAN] HIT! SCLK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=%d ID=%08lX\n",
                        kBusSclkOpts[0], kBusMosiOpts[0], kBusMisoOpts[0],
                        cs, dc, rst, (unsigned long)id);
        }
        if ((tested % 500) == 0) {
          Serial.printf("[V8SCAN] tested %lu combos...\n", (unsigned long)tested);
          delay(1);
        }
      }
    }
  }
  Serial.printf("[V8SCAN] done, %lu combos tested.\n", (unsigned long)tested);
  Serial.println(F("[V8SCAN] Backlight probe: type a GPIO number, it will go HIGH for 3s."));
  Serial.println(F("[V8SCAN] Reply with the pin where the screen lights up."));
}

void loop() {
  // Interactive backlight probe.
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    int pin = s.toInt();
    if (s.length() > 0 && pin >= 0 && pin <= 28) {
      Serial.printf("[V8SCAN] BL probe on GPIO %d for 3s...\n", pin);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      delay(3000);
      digitalWrite(pin, LOW);
      Serial.println(F("[V8SCAN] off. Screen lit? Tell us the pin."));
    } else {
      Serial.println(F("[V8SCAN] enter a GPIO 0..28"));
    }
  }
  delay(50);
}

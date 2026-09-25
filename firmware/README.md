# Firmware

![visitors](https://visitor-badge.laobi.icu/badge?page_id=RocketDevil.PURE-WARDRIVER.firmware)

Ready-to-flash images (e.g. with the
[WEB FLASHER](https://thelastoutpostworkshop.github.io/ESPConnect/)):

- `Pure-Wardrive-V8.bin` — Marauder V8, first install (offset `0x0`,
  erases everything incl. settings)
- `Pure-Wardrive-V8-app.bin` — Marauder V8, update (offset `0x10000`,
  **keeps settings + credentials**)
- `Pure-Wardrive-CP-ADV.bin` — M5 Cardputer ADV, first install (`0x0`)
- `Pure-Wardrive-CP-ADV-app.bin` — Cardputer ADV, update (`0x10000`)
- `Pure-Wardrive-CYD-C5.bin` — NM-CYD-C5, first install (`0x0`, 16 MB flash)
- `Pure-Wardrive-CYD-C5-app.bin` — NM-CYD-C5, update (`0x10000`)

Rule: first install → full `.bin` @`0x0`, every later update → `-app.bin`
@`0x10000`.

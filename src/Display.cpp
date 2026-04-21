// Display.cpp — board-specific display wiring and initialisation.
// The active board is selected at compile time via build flags in platformio.ini:
//   BOARD_WAVESHARE  → Waveshare ESP32-S3-Touch-LCD-1.46B  (SPD2010, QSPI, 412×412)
//   (default)        → upesy_wroom / generic ESP32          (GC9A01,  SPI,  240×240)

#include "Display.h"
#include "config.h"
#include <Arduino.h>

// ── Bus and panel instantiation ───────────────────────────────────────────────
// Both boards use the Arduino_GFX library; only the bus type and panel driver differ.

#ifdef BOARD_WAVESHARE
Arduino_DataBus *_bus = new Arduino_ESP32QSPI(
    WAVESHARE_CS_PIN,  WAVESHARE_SCK_PIN,
    WAVESHARE_D0_PIN,  WAVESHARE_D1_PIN,
    WAVESHARE_D2_PIN,  WAVESHARE_D3_PIN);
Arduino_GFX *gfx = new Arduino_SPD2010(_bus, GFX_NOT_DEFINED);
#else
Arduino_DataBus *_bus = new Arduino_ESP32SPI(
    GC9A01_DC_PIN, GC9A01_CS_PIN, GC9A01_SCK_PIN, GC9A01_MOSI_PIN);
Arduino_GFX *gfx = new Arduino_GC9A01(_bus, GC9A01_RST_PIN);
#endif

// ── TJpg_Decoder callback ─────────────────────────────────────────────────────

// Called by TJpgDec once for every 16×16 pixel tile in the decoded JPEG.
// x/y is the tile's top-left corner on the display; bitmap is row-major RGB565.
// Returning false would abort decoding early — always return true here.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
    return true;
}

// ── Boot status overlay ───────────────────────────────────────────────────────

// Start a quarter of the way down so text lands in the wider middle of the
// round screen, clear of the curved edges where corners get clipped.
static int16_t _statusY = DISPLAY_HEIGHT / 4;

// Draw one centered status line and advance the cursor downward.
// textSize(2): each character is 12 px wide × 16 px tall; lines are 20 px apart.
// Centering is calculated from string length so text stays within the safe
// circular area on both the 240×240 and 412×412 round displays.
// Stops drawing silently once the next line would leave the screen.
void showStatus(const char *msg, uint16_t color) {
    const int16_t charWidth  = 12;  // pixels per character at textSize(2)
    const int16_t lineHeight = 20;
    if (_statusY + lineHeight > DISPLAY_HEIGHT) return;

    int16_t textWidth = strlen(msg) * charWidth;
    int16_t x = (DISPLAY_WIDTH - textWidth) / 2;
    if (x < 0) x = 0;  // clamp if the string is wider than the screen

    gfx->setTextSize(2);
    gfx->setTextColor(color);
    gfx->setCursor(x, _statusY);
    gfx->print(msg);
    _statusY += lineHeight;
}

// ── Public initialisation ─────────────────────────────────────────────────────

void initDisplay() {
#ifdef BOARD_WAVESHARE
    // The SPD2010 panel requires its power rail to be asserted via GPIO before
    // begin() is called; the display will remain dark without this.
    pinMode(WAVESHARE_POWER_PIN, OUTPUT);
    digitalWrite(WAVESHARE_POWER_PIN, HIGH);
#endif

    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
    } else {
        Serial.println("Display init OK");
    }

    gfx->setRotation(DISPLAY_ROTATION);
#ifndef BOARD_WAVESHARE
    gfx->invertDisplay(GC9A01_INVERT_COLORS);
#endif
    gfx->fillScreen(0x0000);  // Black screen while waiting for the first image

#ifdef BOARD_WAVESHARE
    // Enable backlight after begin() so the panel is ready before it lights up.
    pinMode(WAVESHARE_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(WAVESHARE_BACKLIGHT_PIN, HIGH);
#endif
}

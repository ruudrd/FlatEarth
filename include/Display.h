// Display.h — display initialisation and JPEG tile callback.
// Board-specific wiring (pins, bus type, panel driver) is selected at compile
// time via BOARD_WAVESHARE / BOARD_UPESY_WROOM build flags; see Display.cpp.

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino_GFX_Library.h>

// Global display driver instance.
// Defined in Display.cpp; the board-specific bus and panel are wired up there.
// Other modules (e.g. main.cpp) may call gfx methods after initDisplay() succeeds.
extern Arduino_GFX *gfx;

// Initialise the display bus and panel, apply rotation, and clear the screen black.
// Must be called once from setup() before drawing anything.
void initDisplay();

// Print a status line on screen during boot, advancing one line per call.
// Intended for boot-time feedback only; satellite images overwrite it once running.
// color is an RGB565 value — use WHITE (0xFFFF), GREEN (0x07E0), or RED (0xF800).
void showStatus(const char *msg, uint16_t color = 0xFFFF);

// TJpg_Decoder tile callback.
// The decoder calls this once per 16×16 decoded block; this function forwards
// the block to the display via gfx->draw16bitRGBBitmap().
// Returns true to continue decoding the rest of the JPEG.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

#endif

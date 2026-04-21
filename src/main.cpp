// main.cpp — FlatEarth satellite display
//
// Fetches GOES / ElektroL satellite imagery via ImageKit.io, caches it on
// LittleFS, and animates the last 24 hours on a round TFT display.
//
// Boot sequence: display → WiFi → NTP → cache → loop
// Loop:          download latest frame → draw → animate 24 h → wait

#include <Arduino.h>
#include <WiFi.h>
#include <TJpg_Decoder.h>
#include <time.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "Display.h"
#include "WiFiManager.h"
#include "ImageCache.h"
#include "ImageDownloader.h"

// ── Shared image buffer ───────────────────────────────────────────────────────
// Owned here; extern'd in ImageDownloader.h so both the downloader and cache
// can read and write the same allocation without passing pointers everywhere.
uint8_t *imageBuffer = nullptr;
size_t   imageSize   = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Print a one-time hardware summary to Serial so it is easy to confirm the
// correct board and partition layout are in use.
static void printSystemInfo() {
    Serial.println(F("\n##################################"));
    Serial.println(F("ESP32 hardware summary:"));
    Serial.printf("  Heap:    %d total, %d free\n",   ESP.getHeapSize(),      ESP.getFreeHeap());
    Serial.printf("  PSRAM:   %d total, %d free\n",   ESP.getPsramSize(),     ESP.getFreePsram());
    Serial.printf("  Flash:   %d bytes @ %d Hz\n",    ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.printf("  Sketch:  %d bytes, %d free\n",   ESP.getSketchSize(),    ESP.getFreeSketchSpace());
    Serial.printf("  Chip:    %s rev%d @ %d MHz  SDK %s\n",
                  ESP.getChipModel(), ESP.getChipRevision(),
                  ESP.getCpuFreqMHz(), ESP.getSdkVersion());
    Serial.println(F("##################################\n"));
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(SERIAL_SPEED);

#ifdef BOARD_WAVESHARE
    // USB CDC on the ESP32-S3 needs a moment to enumerate before Serial output
    // is visible; without this delay the first log lines are lost.
    delay(3000);
#endif

    // Disable the task watchdog — image downloads can take several seconds and
    // would otherwise trigger a reset on IDF 5.x with the default WDT timeout.
    esp_task_wdt_deinit();

    if (DEBUG_ENABLED) Serial.println("\nFlatEarth display starting...");
    printSystemInfo();

    initDisplay();
    showStatus("FlatEarth starting...");

    // Wire the JPEG decoder to the display callback. setSwapBytes(false) because
    // Arduino_GFX expects native-endian (little-endian) RGB565.
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(tft_output);

    setupWiFi();

    // Synchronise the RTC via NTP. Block until the time is valid so that image
    // URL timestamps are correct from the very first download.
    showStatus("Syncing time...");
    configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);
    struct tm t;
    while (!getLocalTime(&t)) delay(500);
    showStatus("Time OK", 0x07E0);

    showStatus("Init cache...");
    if (cache.begin())
        showStatus("Cache OK", 0x07E0);
    else
        showStatus("Cache FAILED", 0xF800);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (DEBUG_ENABLED) Serial.println("WiFi lost, reconnecting...");
        setupWiFi();
        return;
    }

    // Fetch and display the most recent satellite image.
    String ts = ImageDownloader::getFormattedTime();
    if (ImageDownloader::downloadImage(ts)) {
        if (DEBUG_ENABLED) Serial.println("Drawing latest frame...");
        TJpgDec.drawJpg(0, 0, imageBuffer, imageSize);
    }

    // Play back the last 24 hours as an animation.
    ImageDownloader::showLastXHours();

    // Print cache health to Serial after every full animation cycle so the user
    // can see how full the cache is and whether quality settings need tuning.
    cache.printStats();

    delay(UPDATE_INTERVAL_MS);
}

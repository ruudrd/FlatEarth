// config.h — central configuration for the FlatEarth satellite display project.
// All tuneable parameters live here. Secrets (WiFi credentials, API endpoint)
// are in secrets.h, which is excluded from version control.

#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h"

// ── Device ──────────────────────────────────────────────────────────────────
#define DEVICENAME "ESP-FlatEarth"  // mDNS hostname and WiFi station name

// ── Debug ────────────────────────────────────────────────────────────────────
#define DEBUG_ENABLED true   // Set false to silence all Serial output
#define SERIAL_SPEED  115200

// ── Display ──────────────────────────────────────────────────────────────────
// DISPLAY_WIDTH/DISPLAY_HEIGHT default to 240×240 (upesy_wroom / GC9A01).
// The waveshare build environment overrides these to 412×412 via build flags.
#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH  240
#endif
#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 240
#endif
#define DISPLAY_ROTATION 0  // 0 = normal  1 = 90°  2 = 180°  3 = 270°

// ── Pin assignments — upesy_wroom / GC9A01 240×240 (standard SPI) ───────────
#define GC9A01_INVERT_COLORS true  // Some GC9A01 panels ship with inverted colors; set false if yours looks correct
#define GC9A01_DC_PIN    17  // Data/Command select
#define GC9A01_CS_PIN     5  // Chip select
#define GC9A01_SCK_PIN   18  // SPI clock
#define GC9A01_MOSI_PIN  23  // SPI data out
#define GC9A01_RST_PIN    4  // Hardware reset

// ── Pin assignments — Waveshare ESP32-S3 / SPD2010 412×412 (QSPI) ───────────
#define WAVESHARE_POWER_PIN      7  // GPIO that enables the display power rail
#define WAVESHARE_BACKLIGHT_PIN  5  // GPIO that enables the backlight
#define WAVESHARE_CS_PIN        21
#define WAVESHARE_SCK_PIN       40
#define WAVESHARE_D0_PIN        46
#define WAVESHARE_D1_PIN        45
#define WAVESHARE_D2_PIN        42
#define WAVESHARE_D3_PIN        41

// ── WiFi ─────────────────────────────────────────────────────────────────────
// Credentials (WIFI_SSID1/2, WIFI_PASSWORD1/2) are defined in secrets.h.
#define WIFI_CONNECT_ATTEMPTS  2   // Attempts per network before trying the backup
#define WIFI_CONNECT_DELAY_MS 500   // Delay between each attempt (ms)

// ── Time ─────────────────────────────────────────────────────────────────────
#define NTP_SERVER         "pool.ntp.org"
#define GMT_OFFSET_SEC     0   // UTC offset in seconds (e.g. GMT+2 = 7200).
                               // Keep 0 — satellite timestamps are always UTC.
#define SERVER_LAG_MINUTES 15  // Satellite images lag real-time by ~15 min due
                               // to on-ground processing; subtracted from current
                               // time when building the image URL.

// ── Image download ───────────────────────────────────────────────────────────
#define JPEG_QUALITY         70  // ImageKit resize quality (1–100).
                                 // Lower = smaller files, faster animation.
#define DOWNLOAD_TIMEOUT_MS 5000 // Abort HTTP stream if no data arrives for this long (ms)
#define UPDATE_INTERVAL_MS 10000 // Pause between main loop iterations (ms)

// ── Satellite source ─────────────────────────────────────────────────────────
// Set SATTYPE to choose which satellite feed to display.
// Also update NROFIMAGESTOSHOW and RESIZEURL to match.

#define GOES_EAST 0
#define GOES_WEST 1
#define ELEKTROL  2

#define SATTYPE          GOES_EAST        // Active satellite source
#define NROFIMAGESTOSHOW NROFIMAGES_GOES  // Frames per animation cycle
#define RESIZEURL        RESIZEURL_GOES   // ImageKit subfolder for the chosen source

// Number of images that cover 24 hours for each source
#define NROFIMAGES_GOES     144  // GOES updates every 10 min  → 144 frames / 24 h
#define NROFIMAGES_ELEKTROL  48  // ElektroL updates every 30 min → 48 frames / 24 h

// ImageKit subfolder names (appended to IMAGEKIT_ENDPOINT in the request URL)
#define RESIZEURL_GOES     "GOES/"
#define RESIZEURL_ELEKTROL "ElektroL/"

// NOAA CDN source paths for each GOES satellite
#define BASE_URL_EAST "GOES19/ABI/FD/GEOCOLOR/"
#define BASE_URL_WEST "GOES18/ABI/FD/GEOCOLOR/"

// Resolution of the NOAA source image that ImageKit fetches before resizing.
// Smaller values reduce the downsampling ratio and may improve sharpness on
// small displays; larger values give ImageKit more data to work with.
#define GOES_SOURCE_SIZE 1808  // options: 339 | 678 | 1808 | 5424 | 10848 | 21696

// ── Image cache (LittleFS) ───────────────────────────────────────────────────
// In-memory ring buffer — tracks recently cached timestamps.
// Must be at least NROFIMAGESTOSHOW + 1.
#define CACHE_SIZE 145

// Trigger eviction of the oldest frame when LittleFS reaches this fraction full (percentage).
#define CACHE_FILL_THRESHOLD 0.99f

#endif

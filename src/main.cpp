/**
* GOES-16/17 Satellite Image Display
*
* This program downloads and displays GOES satellite images on a TFT display
* Features include image caching, automatic updates, and 24-hour animation
*/

#include <Arduino.h>
#include <WiFi.h>
#include <TJpg_Decoder.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "ImageCache.h"
#include "ImageDownloader.h"

// Display objects — board-specific wiring, unified library
#ifdef BOARD_WAVESHARE
// Waveshare ESP32-S3-Touch-LCD-1.46B: SPD2010 412x412, QSPI
Arduino_DataBus *_bus = new Arduino_ESP32QSPI(
    21 /* CS */, 40 /* SCK */, 46 /* D0 */, 45 /* D1 */, 42 /* D2 */, 41 /* D3 */);
Arduino_GFX *gfx = new Arduino_SPD2010(_bus, GFX_NOT_DEFINED);
#else
// Generic ESP32 + GC9A01 240x240, standard SPI
Arduino_DataBus *_bus = new Arduino_ESP32SPI(17 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */);
Arduino_GFX *gfx = new Arduino_GC9A01(_bus, 4 /* RST */);
#endif

uint8_t *imageBuffer = NULL;         // Buffer to hold downloaded/cached images
size_t imageSize = 0;                // Size of current image in buffer

/**
* Callback function for TJpg_Decoder
* Pushes decoded image data to the display
*/
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
    return true;
}

/**
* Initialize and manage WiFi connection
* Attempts to connect to primary network, falls back to secondary if needed
*/
void setupWiFi() {
   if (DEBUG_ENABLED)
       Serial.println("Connecting to primary WiFi...");
   WiFi.setHostname(DEVICENAME);
   WiFi.begin(WIFI_SSID1, WIFI_PASSWORD1);

   int attempts = 0;
   while (WiFi.status() != WL_CONNECTED && attempts < 20) {
       delay(500);
       if (DEBUG_ENABLED)
           Serial.print(".");
       attempts++;
   }

   if (WiFi.status() != WL_CONNECTED) {
       if (DEBUG_ENABLED)
           Serial.println("\nTrying backup WiFi...");
       WiFi.begin(WIFI_SSID2, WIFI_PASSWORD2);
       attempts = 0;

       while (WiFi.status() != WL_CONNECTED && attempts < 20) {
           delay(500);
           if (DEBUG_ENABLED)
               Serial.print(".");
           attempts++;
       }
   }

   if (WiFi.status() == WL_CONNECTED && DEBUG_ENABLED) {
       Serial.println("\nWiFi connected!");
   }
}

/**
* System initialization
*/
void setup() {
   Serial.begin(SERIAL_SPEED);
#ifdef BOARD_WAVESHARE
   delay(3000);  // USB CDC needs time to establish connection on ESP32-S3
#endif
   esp_task_wdt_deinit();  // Disable task watchdog — long downloads trip it on IDF 5.x
   if (DEBUG_ENABLED)
       Serial.println("\nGOES-16 Display starting...");

   Serial.println(F("\n##################################"));
   Serial.println(F("ESP32 Information:"));
   Serial.printf("Internal Total Heap %d, Internal Used Heap %d, Internal Free Heap %d\n",
                 ESP.getHeapSize(), ESP.getHeapSize() - ESP.getFreeHeap(), ESP.getFreeHeap());
   Serial.printf("Sketch Size %d, Free Sketch Space %d\n",
                 ESP.getSketchSize(), ESP.getFreeSketchSpace());
   Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n",
                 ESP.getPsramSize(), ESP.getFreePsram());
   Serial.printf("Chip Model %s, ChipRevision %d, Cpu Freq %d, SDK Version %s\n",
                 ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
   Serial.printf("Flash Size %d, Flash Speed %d\n",
                 ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
   Serial.println(F("##################################\n\n"));

   delay(1000);

   // Initialize display
#ifdef BOARD_WAVESHARE
   pinMode(7, OUTPUT);
   digitalWrite(7, HIGH);  // Power enable
#endif
   if (!gfx->begin()) {
       Serial.println("ERROR: Display init failed!");
   } else {
       Serial.println("Display init OK");
   }
   gfx->setRotation(DISPLAY_ROTATION);
   gfx->fillScreen(0x0000);
#ifdef BOARD_WAVESHARE
   pinMode(5, OUTPUT);
   digitalWrite(5, HIGH);  // Backlight
#endif

   // Setup JPEG decoder
   TJpgDec.setSwapBytes(false);  // Arduino_GFX expects native-endian RGB565
   TJpgDec.setCallback(tft_output);

   // Initialize network and time
   setupWiFi();

   struct tm tempTimeinfo;
   configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);
   Serial.println("Waiting for time to be set");
   while (!getLocalTime(&tempTimeinfo)) {
       Serial.print(".");
   }

   // Initialize image cache
   cache.begin();
   cache.cleanup();
}

/**
* Main program loop
*/
void loop() {
   if (WiFi.status() == WL_CONNECTED) {
       if (ImageDownloader::downloadImage(ImageDownloader::getFormattedTime())) {
           if (DEBUG_ENABLED) Serial.println("Drawing image...");
           TJpgDec.drawJpg(0, 0, imageBuffer, imageSize);
           if (DEBUG_ENABLED) Serial.println("Image drawn");
       }
   } else {
       if (DEBUG_ENABLED) Serial.println("WiFi disconnected, attempting reconnect...");
       setupWiFi();
   }

   if (DEBUG_ENABLED) Serial.println("Waiting for next update...");
   ImageDownloader::showLastXHours();
   delay(UPDATE_INTERVAL_MS);
}

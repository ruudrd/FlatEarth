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
#ifdef USE_ARDUINO_GFX
#include <Arduino_GFX_Library.h>
#else
#include <TFT_eSPI.h>
#endif
#include <time.h>
#include "config.h"
#include "ImageCache.h"
#include "ImageDownloader.h"

// Global instances and variables
#ifdef USE_ARDUINO_GFX
// Waveshare ESP32-S3-Touch-LCD-1.46B: SPD2010 QSPI display, 412x412
Arduino_DataBus *_bus = new Arduino_ESP32QSPI(
    21 /* CS */, 40 /* SCK */, 46 /* D0 */, 45 /* D1 */, 42 /* D2 */, 41 /* D3 */);
Arduino_GFX *gfx = new Arduino_SPD2010(_bus, GFX_NOT_DEFINED);
#else
TFT_eSPI tft = TFT_eSPI();          // TFT display instance
#endif
uint8_t *imageBuffer = NULL;         // Buffer to hold downloaded/cached images
size_t imageSize = 0;                // Size of current image in buffer

/**
* Callback function for TJpg_Decoder
* Pushes decoded image data to the TFT display
*/
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
#ifdef USE_ARDUINO_GFX
    gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
#else
    tft.pushImage(x, y, w, h, bitmap);
#endif
    return true;
}

/**
* Initialize and manage WiFi connection
* Attempts to connect to primary network, falls back to secondary if needed
*/
void setupWiFi() {
   if (DEBUG_ENABLED)
       Serial.println("Connecting to primary WiFi...");
   WiFi.setHostname(DEVICENAME);    // Set device name in network
   WiFi.begin(WIFI_SSID1, WIFI_PASSWORD1);

   // Try primary WiFi connection
   int attempts = 0;
   while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 10 seconds timeout
       delay(500);
       if (DEBUG_ENABLED)
           Serial.print(".");
       attempts++;
   }

   // If primary fails, try backup WiFi
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
* Sets up all required hardware and connections
*/
void setup() {
   // Initialize serial debugging if enabled
   Serial.begin(SERIAL_SPEED);
#ifdef USE_ARDUINO_GFX
   delay(3000);  // USB CDC needs time to establish connection on ESP32-S3
#endif
   if (DEBUG_ENABLED) {
       Serial.println("\nGOES-16 Display starting...");
   }

   // Print ESP32 system information
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
#ifdef USE_ARDUINO_GFX
   pinMode(7, OUTPUT);
   digitalWrite(7, HIGH);  // Power enable
   if (!gfx->begin()) {
       Serial.println("ERROR: Display init failed!");
   } else {
       Serial.println("Display init OK");
   }
   gfx->setRotation(DISPLAY_ROTATION);
   gfx->fillScreen(0xF800);  // Red — visual confirmation display is alive
   pinMode(5, OUTPUT);
   digitalWrite(5, HIGH);  // Backlight
#else
   tft.init();
   tft.setRotation(DISPLAY_ROTATION);
   tft.fillScreen(TFT_BLACK);
#endif

   // Setup JPEG decoder
#ifdef USE_ARDUINO_GFX
    TJpgDec.setSwapBytes(false);
#else
    TJpgDec.setSwapBytes(true);
#endif
   TJpgDec.setCallback(tft_output);

   // Initialize network and time
   setupWiFi();

   //Initialize time
   struct tm tempTimeinfo;  //create timepeice
   configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);
   Serial.println("Waiting for time to be set");
    while(!getLocalTime(&tempTimeinfo)){  //Get the time from the NTP server
       Serial.print("."); //Show a dot for every attempt, its very slow
    }
   
   // Initialize image cache
   cache.begin();
   cache.cleanup();  //maybe only needed for testing...
}

/**
* Main program loop
* Handles image updates and display
*/
void loop() {
   // Check WiFi connection
   if (WiFi.status() == WL_CONNECTED) {
       // Download and display latest image
       if (ImageDownloader::downloadImage(ImageDownloader::getFormattedTime())) {
           if (DEBUG_ENABLED) Serial.println("Drawing image...");
           TJpgDec.drawJpg(0, 0, imageBuffer, imageSize);
           if (DEBUG_ENABLED) Serial.println("Image drawn");
       }
   } else {
       // Attempt to reconnect if WiFi is lost
       if (DEBUG_ENABLED) Serial.println("WiFi disconnected, attempting reconnect...");
       setupWiFi();
   }

   if (DEBUG_ENABLED) Serial.println("Waiting for next update...");
   ImageDownloader::showLastXHours();  // Show animation of last 24 hours
   delay(UPDATE_INTERVAL_MS);           // Wait before next update
}
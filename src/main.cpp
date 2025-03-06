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
#include <TFT_eSPI.h>
#include <time.h>
#include "config.h"
#include "ImageCache.h"
#include "ImageDownloader.h"

// Global instances and variables
TFT_eSPI tft = TFT_eSPI();          // TFT display instance
uint8_t *imageBuffer = NULL;         // Buffer to hold downloaded/cached images
size_t imageSize = 0;                // Size of current image in buffer

/**
* Callback function for TJpg_Decoder
* Pushes decoded image data to the TFT display
*/
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
   tft.pushImage(x, y, w, h, bitmap);
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
   tft.init();
   tft.setRotation(DISPLAY_ROTATION);
   tft.fillScreen(TFT_BLACK);

   // Setup JPEG decoder
   TJpgDec.setSwapBytes(true);
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
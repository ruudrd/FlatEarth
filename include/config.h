// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h" //Include the secrets.h file for the IMAGEKIT_ENDPOINT and wifi settings.

#define DEVICENAME "ESP-FlatEarth"

// Debug Settings
#define DEBUG_ENABLED true
#define SERIAL_SPEED 115200

// Display Settings
#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH 240
#endif
#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 240
#endif
#define DISPLAY_ROTATION 0

// For refrence, this needs to be set in de User setup.h
// #define GC9A01_DRIVER
//  //#define TFT_MISO 19  // (leave TFT SDO disconnected if other SPI devices share MISO)
//  #define TFT_MOSI 23
//  #define TFT_SCLK 18
//  #define TFT_CS   5  // Chip select control pin
//  #define TFT_DC    17  // Data Command control pin
//  #define TFT_RST   4  // Reset pin (could connect to RST pin)
// #define SPI_FREQUENCY  40000000
// #define SPI_READ_FREQUENCY  20000000 // Optional reduced SPI frequency for reading TFT

// Time Settings
#define NTP_SERVER "pool.ntp.org" // NTP server to use for time synchronization
#define GMT_OFFSET_SEC 0          // set this to 0 and use the time as is, or if you are on the other side of the world, set this to the offset in seconds from GMT (e.g. for GMT+2, use 2*3600)
#define UPDATE_INTERVAL_MS 10000  // 10 minutes

#define SATTYPE GOES_EAST                // Choose which dataset to show, this will determine the number of images to show in the animation and the URL to fetch the images from
#define NROFIMAGESTOSHOW NROFIMAGES_GOES // Depending on the chosen dataset, we can show an animation of every 20 minutes, for GOES or 30 minutes for ElektroL. The number of images to show for 24 hours is 72 for GOES and 48 for ElektroL. The EPIC has one image every hour, so we can show 24 images
#define RESIZEURL RESIZEURL_GOES

// GOES Image Settings

#define GOES_EAST 0
#define GOES_WEST 1
#define ELEKTROL 2

#define RESIZEURL_ELEKTROL "ElektroL/"
#define RESIZEURL_GOES "GOES/"
#define RESIZEURL_EPIC "EPIC/"
#define BASE_URL_EAST "GOES19/ABI/FD/GEOCOLOR/"
#define BASE_URL_WEST "GOES18/ABI/FD/GEOCOLOR/"
#define IMAGE_SUFFIX_EAST "_GOES19-ABI-FD-GEOCOLOR-1808x1808.jpg" // 5424x5424.jpg"
#define IMAGE_SUFFIX_WEST "_GOES18-ABI-FD-GEOCOLOR-1808x1808.jpg" // 5424x5424.jpg"

// Depending on the chosen dataset, we can show an animation of
// Every 10 minutes for GOES or 30 minutes for ElektroL
// The number of images to show for 24 hours is 144 for GOES and 48 for ElektroL
// The EPIC has one image every hour, so we can show 24 images
#define NROFIMAGES_GOES 144
#define NROFIMAGES_ELEKTROL 48
#define NROFIMAGES_EPIC 24

#define CACHE_SIZE 145

#endif

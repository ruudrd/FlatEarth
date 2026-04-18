// ImageDownloader.h — satellite image retrieval and 24-hour animation playback.

#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "ImageCache.h"

// Shared image buffer, defined in main.cpp.
// Both downloadImage() (HTTP) and loadImage() (LittleFS) write their result here
// so the caller always draws from the same location regardless of source.
extern uint8_t *imageBuffer;
extern size_t   imageSize;

class ImageDownloader {
public:
    // Fetch the satellite image for the given timestamp.
    // Checks the LittleFS cache first; only downloads from the network on a miss.
    // On success, imageBuffer and imageSize hold the complete JPEG.
    // Returns true if the image is ready to be decoded and drawn.
    static bool   downloadImage(const String& timestamp);

    // Return the timestamp string for the most recently available satellite image.
    // Subtracts SERVER_LAG_MINUTES to account for satellite processing delay, then
    // rounds down to the nearest update cadence (10 min for GOES, 30 min for ElektroL).
    static String getFormattedTime();

    // Play back 24 hours of satellite imagery as a frame-by-frame animation.
    // Starting from (now − SERVER_LAG_MINUTES − 24 h), iterates forward through
    // NROFIMAGESTOSHOW evenly-spaced timestamps, downloading and immediately
    // drawing each frame. Cache hits make this fast; misses trigger HTTP downloads.
    static void   showLastXHours();

private:
    // Build the complete ImageKit URL for the given timestamp and active SATTYPE.
    // Embeds DISPLAY_WIDTH, DISPLAY_HEIGHT, and JPEG_QUALITY as resize parameters.
    static String constructUrl(const String& timestamp);

    // Convert a tm struct to the satellite-source-specific timestamp string.
    // GOES:    YYYYDDDHHMM  (day-of-year, minutes rounded to nearest 10)
    // ElektroL: YYYYMMDD-HHMM (minutes rounded to nearest 30)
    static String formatTimestamp(const struct tm& t);
};

#endif

#include "ImageDownloader.h"
#include "config.h"
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>

// External reference to TFT display instance
extern TFT_eSPI tft;

/**
 * Download or load from cache a GOES satellite image for a specific timestamp
 * @param timestamp The formatted timestamp string for the image
 * @return true if image was successfully retrieved, false otherwise
 */
bool ImageDownloader::downloadImage(String timestamp)
{

    // Try loading from cache first
    if (cache.loadImage(timestamp))
    {
        if (DEBUG_ENABLED)
            Serial.print("Cache! ");
        return true;
    }

    if (DEBUG_ENABLED)
        Serial.println("Starting download...");

    HTTPClient http;
    bool downloadSuccess = false;
    String url = constructUrl(timestamp);

    if (DEBUG_ENABLED)
    {
        Serial.print("URL: ");
        Serial.println(url);
    }

    // Initialize HTTP connection
    http.begin(url);
    int httpCode = http.GET();

    // Check if HTTP request was successful
    if (httpCode != HTTP_CODE_OK)
    {
        if (DEBUG_ENABLED)
        {
            Serial.print("HTTP GET failed, error: ");
            Serial.println(httpCode);
        }
        http.end();
        return false;
    }

    // Free previous image buffer if it exists
    if (imageBuffer != NULL)
    {
        free(imageBuffer);
    }

    // Get size of incoming image
    imageSize = http.getSize();
    if (DEBUG_ENABLED)
    {
        Serial.print("Image size: ");
        Serial.println(imageSize);
    }

    // Allocate memory for new image
    imageBuffer = (uint8_t *)malloc(imageSize);
    if (!imageBuffer)
    {
        if (DEBUG_ENABLED)
            Serial.println("Memory allocation failed");
        http.end();
        return false;
    }

    // Download image data in chunks
    WiFiClient *stream = http.getStreamPtr();
    size_t read = 0;
    while (read < imageSize)
    {
        size_t available = stream->available();
        if (available)
        {
            read += stream->readBytes(imageBuffer + read, available);
        }
    }

    http.end();

    // Cache the image if download was successful
    if (read == imageSize)
    {
        cache.cacheImage(timestamp);
        downloadSuccess = true;
        if (DEBUG_ENABLED)
            Serial.println("Download complete");
    }

    return downloadSuccess;
}

/**
 * Construct the URL for the image based on satellite type and timestamp
 * @param timestamp The formatted timestamp string
 * @return Complete URL for image download
 */
String ImageDownloader::constructUrl(String timestamp)
{
    String url;
    switch (SATTYPE)
    {
    case GOES_EAST: // EAST
        url = String(IMAGEKIT_ENDPOINT) + String(RESIZEURL) +
              "tr:w-" + String(DISPLAY_WIDTH) + ",h-" + String(DISPLAY_HEIGHT) + ",q-75" + "/" +
              String(BASE_URL_EAST) +
              timestamp +
              String(IMAGE_SUFFIX_EAST);
        break;
    case GOES_WEST: // WEST
        url = String(IMAGEKIT_ENDPOINT) + String(RESIZEURL) +
              "tr:w-" + String(DISPLAY_WIDTH) + ",h-" + String(DISPLAY_HEIGHT) + ",q-75" + "/" +
              String(BASE_URL_WEST) +
              timestamp +
              String(IMAGE_SUFFIX_WEST);
        break;
    case ELEKTROL: // ELEKTROL uses dates 20250109-0630.jpg
        url = String(IMAGEKIT_ENDPOINT) + String(RESIZEURL_ELEKTROL) +
              "tr:w-" + String(DISPLAY_WIDTH) + ",h-" + String(DISPLAY_HEIGHT) + ",q-75" + "/" +
              timestamp + ".jpg";

        break;
    }
    return url;
}

/**
 * Generate a formatted timestamp string for the current time
 * Adjusts for server time offset and rounds to nearest 10 minutes
 * @return Formatted timestamp string in YYYYDDDHHMM format
 */
String ImageDownloader::getFormattedTime()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        if (DEBUG_ENABLED)
            Serial.println("Failed to get time");
        return "";
    }

    // Adjust for server time offset
    timeinfo.tm_min -= 15;
    mktime(&timeinfo);

    // Format timestamp string (YYYYDDDHHMM)
    char timeStr[22];
    switch (SATTYPE)
    {

    case ELEKTROL: // ELEKTROL   20250109-0630.jpg

        snprintf(timeStr, sizeof(timeStr), "%04d%02d%02d-%02d%02d",
                 timeinfo.tm_year + 1900,      // Year
                 timeinfo.tm_mon + 1,          // Month
                 timeinfo.tm_mday,             // Day
                 timeinfo.tm_hour,             // Hour
                 (timeinfo.tm_min / 30) * 30); // Minutes (0 or 30)

        break;

    case GOES_EAST: // GOES EAST
    case GOES_WEST: // GOES WEST

        snprintf(timeStr, sizeof(timeStr), "%d%03d%04d",
                 timeinfo.tm_year + 1900,          // Year
                 timeinfo.tm_yday + 1,             // Day of year (1-366)
                 (timeinfo.tm_hour * 100) +        // Hours (HHMM format)
                     (timeinfo.tm_min / 10) * 10); // Minutes rounded to nearest 10
        break;
    }

    if (DEBUG_ENABLED)
    {
        Serial.printf("Time: %02d:%02d, Day: %d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_yday + 1);
        Serial.printf("Generated timestamp: %s\n", timeStr);
    }
    return String(timeStr);
}

/**
 * Display images from the last 24 hours in sequence
 * Downloads NROFIMAGESTOSHOW images (one every 10 minutes) and displays them
 */
void ImageDownloader::showLastXHours()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        if (DEBUG_ENABLED)
            Serial.println("Failed to get time");
        return;
    }

    // Adjust for server time offset
    timeinfo.tm_min -= 15;
    // Move back in time NRFOIMAGESTOSHOW 10-minute intervals
    switch (SATTYPE)
    {
    case ELEKTROL: // ELEKTROL   20250109-0630.jpg
        timeinfo.tm_min -= (NROFIMAGESTOSHOW - 1) * 30;
        break;
    case GOES_EAST: // GOES EAST
    case GOES_WEST: // GOES WEST
        timeinfo.tm_min -= (NROFIMAGESTOSHOW - 1) * 20;
        break;
    }

    mktime(&timeinfo);

    // Loop through NROFIMAGESTOSHOW 10-minute intervals (24 hours)
    for (int i = 0; i < NROFIMAGESTOSHOW; i++)
    { // Run once more to get the latest image
        char timeStr[22];

        switch (SATTYPE)
        {

        case ELEKTROL: // ELEKTROL   20250109-0630.jpg

            snprintf(timeStr, sizeof(timeStr), "%04d%02d%02d-%02d%02d",
                     timeinfo.tm_year + 1900,      // Year
                     timeinfo.tm_mon + 1,          // Month
                     timeinfo.tm_mday,             // Day
                     timeinfo.tm_hour,             // Hour
                     (timeinfo.tm_min / 30) * 30); // Minutes (0 or 30)

            break;

        case GOES_EAST: // GOES EAST
        case GOES_WEST: // GOES WEST

            snprintf(timeStr, sizeof(timeStr), "%d%03d%04d",
                     timeinfo.tm_year + 1900,          // Year
                     timeinfo.tm_yday + 1,             // Day of year (1-366)
                     (timeinfo.tm_hour * 100) +        // Hours (HHMM format)
                         (timeinfo.tm_min / 10) * 10); // Minutes rounded to nearest 10
            break;
        }
        if (DEBUG_ENABLED)
        {
            Serial.printf("I %d/%d: %s\n", i + 1, NROFIMAGESTOSHOW, timeStr);
        }

        // Download and display each image
        if (downloadImage(String(timeStr)))
        {
            TJpgDec.drawJpg(0, 0, imageBuffer, imageSize);
        }

        // Move foreward 10 minutes for next image
        timeinfo.tm_min += 10;
        mktime(&timeinfo);
    }
}
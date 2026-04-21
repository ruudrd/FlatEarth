// ImageDownloader.cpp — satellite image retrieval and 24-hour animation playback.

#include "ImageDownloader.h"
#include "config.h"
#include <TJpg_Decoder.h>

// ── Private helpers ───────────────────────────────────────────────────────────

// Format a normalised tm struct as the timestamp string for the active source.
// GOES East/West: "YYYYDDDHHMM"   — day-of-year, minutes snapped to nearest 10.
// ElektroL:       "YYYYMMDD-HHMM" — calendar date, minutes snapped to nearest 30.
// Meteosat:       "YYYYMMDD-HHMM" — calendar date, minutes snapped to nearest 15.
//                 Used only as a cache key; the download URL is always the static
//                 "latest" EUMETSAT image and does not embed the timestamp.
String ImageDownloader::formatTimestamp(const struct tm &t)
{
    char buf[22];
    switch (SATTYPE)
    {
    case ELEKTROL:
        snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d",
                 t.tm_year + 1900,
                 t.tm_mon + 1,
                 t.tm_mday,
                 t.tm_hour,
                 (t.tm_min / 30) * 30); // snap to 0 or 30
        break;
    case METEOSAT:
    case METEOSAT_IODC:
        snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d00",
                 t.tm_year + 1900,
                 t.tm_mon + 1,
                 t.tm_mday,
                 t.tm_hour); // snap to hour boundary — low-res image updates every 60 min
        break;
    case GOES_EAST:
    case GOES_WEST:
    default:
        snprintf(buf, sizeof(buf), "%d%03d%04d",
                 t.tm_year + 1900,
                 t.tm_yday + 1,                           // day-of-year, 1-based
                 t.tm_hour * 100 + (t.tm_min / 10) * 10); // HHMM, snapped to 10-min
        break;
    }
    return String(buf);
}

// Build the full ImageKit proxy URL for a given timestamp.
// ImageKit applies the resize transform (width, height, quality) server-side
// before returning the JPEG, so the ESP32 never handles the full-res image.
String ImageDownloader::constructUrl(const String &timestamp)
{
    String resize = "tr:w-" + String(DISPLAY_WIDTH) +
                    ",h-" + String(DISPLAY_HEIGHT) +
                    ",q-" + String(JPEG_QUALITY) + "/";
    switch (SATTYPE)
    {
    case GOES_EAST:
    {
        String suffix = "_GOES19-ABI-FD-GEOCOLOR-" + String(GOES_SOURCE_SIZE) +
                        "x" + String(GOES_SOURCE_SIZE) + ".jpg";
        return String(IMAGEKIT_ENDPOINT) + RESIZEURL + resize +
               BASE_URL_EAST + timestamp + suffix;
    }
    case GOES_WEST:
    {
        String suffix = "_GOES18-ABI-FD-GEOCOLOR-" + String(GOES_SOURCE_SIZE) +
                        "x" + String(GOES_SOURCE_SIZE) + ".jpg";
        return String(IMAGEKIT_ENDPOINT) + RESIZEURL + resize +
               BASE_URL_WEST + timestamp + suffix;
    }
    case ELEKTROL:
        return String(IMAGEKIT_ENDPOINT) + RESIZEURL_ELEKTROL + resize +
               timestamp + ".jpg";
    case METEOSAT:
    case METEOSAT_IODC:
        // EUMETSAT publishes one static filename per satellite that is overwritten
        // in-place every 15 minutes. Without intervention, ImageKit's CDN would
        // serve the same cached copy for every request, making all 96 animation
        // frames identical.
        //
        // `ik-cache-bust` is ImageKit's own cache-bypass parameter: when present,
        // ImageKit skips its CDN cache and fetches a fresh copy from the origin
        // (EUMETSAT) before storing the result under the new cache key. Using the
        // 15-minute timestamp as the bust value means:
        //   - Each slot gets its own ImageKit cache entry → one fresh origin fetch.
        //   - Subsequent requests for the same slot hit ImageKit's cache → fast.
        //
        // The dash is stripped ("20260421-1200" → "202604211200") because some URL
        // parsers treat a bare dash as a separator and may truncate the value,
        // which would make all slots within the same day share the same bust value.
    {
        // Crop out EUMETSAT's border/label area before resizing to the display
        // dimensions. METEOSAT_CROP_SIZE defines the intermediate square extracted
        // from the centre of the full image; ImageKit then scales it down to
        // DISPLAY_WIDTH × DISPLAY_HEIGHT. Adjust METEOSAT_CROP_SIZE in config.h
        // to tighten or loosen the crop without changing any other settings.
        String meteosatResize = "tr:w-" + String(METEOSAT_CROP_SIZE) +
                                ",h-" + String(METEOSAT_CROP_SIZE) +
                                ",cm-extract:w-" + String(DISPLAY_WIDTH) +
                                ",h-" + String(DISPLAY_HEIGHT) +
                                ",q-" + String(JPEG_QUALITY) + "/";
        String bust = timestamp;
        bust.replace("-", "");
        const char* imageFile = (SATTYPE == METEOSAT_IODC)
            ? METEOSAT_IODC_IMAGE_FILE
            : METEOSAT_IMAGE_FILE;
        const char* resizeUrl = (SATTYPE == METEOSAT_IODC)
            ? RESIZEURL_METEOSAT_IODC
            : RESIZEURL_METEOSAT;
        return String(IMAGEKIT_ENDPOINT) + resizeUrl + meteosatResize +
               imageFile + "?ik-cache-bust=" + bust;
    }
    default:
        return "";
    }
}

// ── Public methods ────────────────────────────────────────────────────────────

// Fetch the image for the given timestamp.
// Cache hit: loads the JPEG from LittleFS into imageBuffer — no network call.
// Cache miss: opens an HTTP connection, downloads the JPEG into imageBuffer,
//             then writes the result to LittleFS for future cache hits.
bool ImageDownloader::downloadImage(const String &timestamp)
{
    if (cache.loadImage(timestamp))
    {
        if (DEBUG_ENABLED)
            Serial.print("Cache! ");
        return true;
    }

    String url = constructUrl(timestamp);
    if (DEBUG_ENABLED)
    {
        Serial.print("URL: ");
        Serial.println(url);
    }

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        if (DEBUG_ENABLED)
        {
            Serial.print("HTTP error: ");
            Serial.println(httpCode);
        }
        http.end();
        return false;
    }

    imageSize = http.getSize();
    if (DEBUG_ENABLED)
    {
        Serial.print("Image size: ");
        Serial.println(imageSize);
    }

    if (imageBuffer)
        free(imageBuffer);
    imageBuffer = (uint8_t *)malloc(imageSize);
    if (!imageBuffer)
    {
        if (DEBUG_ENABLED)
            Serial.println("malloc failed");
        http.end();
        return false;
    }

    // Read the stream in chunks, yielding to FreeRTOS between chunks so the
    // task watchdog is not starved during slow downloads.
    WiFiClient *stream = http.getStreamPtr();
    size_t bytesRead = 0;
    unsigned long lastActivity = millis();

    while (bytesRead < imageSize)
    {
        size_t available = stream->available();
        if (available)
        {
            bytesRead += stream->readBytes(imageBuffer + bytesRead, available);
            lastActivity = millis();
        }
        else
        {
            if (millis() - lastActivity > DOWNLOAD_TIMEOUT_MS)
            {
                if (DEBUG_ENABLED)
                    Serial.println("Download timeout");
                break;
            }
            delay(1); // yield
        }
    }

    http.end();

    if (bytesRead != imageSize)
        return false;

    cache.cacheImage(timestamp);
    if (DEBUG_ENABLED)
        Serial.println("Download complete");
    return true;
}

// Return the timestamp string for the most recent available satellite image.
// Subtracts SERVER_LAG_MINUTES from the current UTC time to compensate for the
// delay between image capture and CDN availability, then snaps to the cadence.
String ImageDownloader::getFormattedTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        if (DEBUG_ENABLED)
            Serial.println("Failed to get time");
        return "";
    }
    timeinfo.tm_min -= SERVER_LAG_MINUTES;
    mktime(&timeinfo); // normalise (handles minute underflow across hour boundaries)

    if (DEBUG_ENABLED)
    {
        Serial.printf("Time: %02d:%02d, Day: %d\n",
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_yday + 1);
    }
    return formatTimestamp(timeinfo);
}

// Iterate forward through all NROFIMAGESTOSHOW frames of the 24-hour window,
// downloading and immediately drawing each one. The window starts at
// (now − SERVER_LAG_MINUTES − (NROFIMAGESTOSHOW−1) × step) so that the last
// frame shown is always the most recently available image.
void ImageDownloader::showLastXHours()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        if (DEBUG_ENABLED)
            Serial.println("Failed to get time");
        return;
    }

    // Apply the same server-lag correction used in getFormattedTime().
    timeinfo.tm_min -= SERVER_LAG_MINUTES;

    // Rewind to the start of the animation window.
    int stepMinutes;
    switch (SATTYPE)
    {
    case ELEKTROL:
        stepMinutes = 30;
        break;
    case METEOSAT:
    case METEOSAT_IODC:
        stepMinutes = 60; // low-res image updates every 60 min
        break;
    case GOES_EAST:
    case GOES_WEST:
    default:
        stepMinutes = 10;
        break;
    }
    timeinfo.tm_min -= (NROFIMAGESTOSHOW - 1) * stepMinutes;
    mktime(&timeinfo);

    for (int i = 0; i < NROFIMAGESTOSHOW; i++)
    {
        String ts = formatTimestamp(timeinfo);

        // Meteosat: only play back what is already cached — downloading on a cache
        // miss would fetch "latest" into a historical slot, which is wrong.
        bool loaded = (SATTYPE == METEOSAT || SATTYPE == METEOSAT_IODC)
                          ? cache.loadImage(ts)
                          : downloadImage(ts);
        if (loaded)
        {
            if (DEBUG_ENABLED)
                Serial.printf("Frame %d/%d: %s  Size: %d byte\n",
                              i + 1, NROFIMAGESTOSHOW, ts.c_str(), imageSize);
            TJpgDec.drawJpg(0, 0, imageBuffer, imageSize);
            delay(FRAME_DELAY_MS);
        }
        else
        {
            if (DEBUG_ENABLED)
                Serial.printf("Frame %d/%d: %s  MISS\n",
                              i + 1, NROFIMAGESTOSHOW, ts.c_str());
        }

        timeinfo.tm_min += stepMinutes;
        mktime(&timeinfo);
    }
}

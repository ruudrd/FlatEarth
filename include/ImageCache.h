// ImageCache.h — LittleFS-backed JPEG frame cache.
// Cached files live in /cache/<timestamp>.jpg on the LittleFS partition.
// Filenames sort lexicographically in chronological order, which lets the
// cleanup routine identify the oldest frame without storing a separate index.

#ifndef IMAGE_CACHE_H
#define IMAGE_CACHE_H

#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"

class ImageCache {
public:
    // Mount LittleFS (auto-formatting if the first mount fails) and reset the
    // in-memory ring buffer. Must be called once from setup() before any other
    // cache methods. Returns true on success.
    bool begin();

    // Write the current imageBuffer / imageSize to LittleFS under <timestamp>.jpg.
    // If storage is at or above CACHE_FILL_THRESHOLD, evicts the oldest frame first.
    // Returns true if the file was written successfully.
    bool cacheImage(const String& timestamp);

    // Load the cached JPEG for <timestamp> into imageBuffer / imageSize.
    // Returns false if the file is not found or is corrupt (zero-length).
    bool loadImage(const String& timestamp);

    // Evict the single oldest file from /cache/ to free space for one incoming image.
    // Selects the oldest file by scanning for the lexicographically smallest filename.
    void cleanup();

private:
    // Metadata entry for the in-memory ring buffer.
    struct CacheEntry {
        String timestamp;
        bool   valid;
    };

    CacheEntry entries[CACHE_SIZE];  // Ring buffer of recently cached timestamps
    int        writeIndex = 0;       // Next write position in the ring buffer

    // Returns the full LittleFS path for a given timestamp, e.g. /cache/20261081300.jpg.
    // Creates the /cache/ directory if it does not yet exist.
    String getCachePath(const String& timestamp);
};

// Global cache instance, defined in ImageCache.cpp.
extern ImageCache cache;

#endif

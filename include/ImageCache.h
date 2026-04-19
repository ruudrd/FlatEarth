// ImageCache.h — LittleFS-backed JPEG frame cache.
// Cached files live in /cache/<satellite>/<timestamp>.jpg on the LittleFS partition.
// Filenames sort lexicographically in chronological order, which lets the
// cleanup routine identify the oldest frame without storing a separate index.
// The satellite subdirectory prevents frames from different sources mixing when
// SATTYPE is changed between builds.

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

    // Evict the oldest cached frame (across ALL satellite subdirectories) to free
    // space for one incoming image. Global scope ensures stale files from a
    // previously active satellite never block eviction for the current one.
    void cleanup();

    // Print a cache health summary to Serial: frame count, average frame size,
    // LittleFS used/free, and a suggestion if quality could be raised or lowered.
    // Call this after showLastXHours() to give feedback on how full the cache is.
    void printStats();

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

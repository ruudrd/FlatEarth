// ImageCache.cpp — LittleFS-backed JPEG frame cache.

#include "ImageCache.h"
#include "config.h"

// imageBuffer and imageSize are owned by main.cpp and shared across all modules.
extern uint8_t *imageBuffer;
extern size_t   imageSize;

// Single global instance used by ImageDownloader and main.
ImageCache cache;

// ── Public methods ────────────────────────────────────────────────────────────

// Mount LittleFS. If the first mount attempt fails (e.g. after a power loss that
// left the filesystem in a bad state), format and retry once. Initialises the
// in-memory ring buffer to all-invalid so loadImage() falls through to the
// filesystem on every first lookup after a reboot.
bool ImageCache::begin() {
    if (DEBUG_ENABLED) Serial.println("Initializing cache system...");

    if (!LittleFS.begin(true)) {
        if (DEBUG_ENABLED) Serial.println("LittleFS mount failed, attempting format...");
        if (!LittleFS.format() || !LittleFS.begin()) {
            if (DEBUG_ENABLED) Serial.println("LittleFS format/mount failed");
            return false;
        }
    }

    if (DEBUG_ENABLED) {
        Serial.printf("LittleFS: %d bytes total, %d used, %d free\n",
                      LittleFS.totalBytes(), LittleFS.usedBytes(),
                      LittleFS.totalBytes() - LittleFS.usedBytes());
    }

    for (int i = 0; i < CACHE_SIZE; i++) entries[i].valid = false;

    if (DEBUG_ENABLED) Serial.println("Cache system initialized successfully");
    return true;
}

// Look up and return /cache/<timestamp>.jpg, creating /cache/ if needed.
String ImageCache::getCachePath(const String& timestamp) {
    if (!LittleFS.exists("/cache")) LittleFS.mkdir("/cache");
    return "/cache/" + timestamp + ".jpg";
}

// Write imageBuffer to LittleFS. If the filesystem is nearly full, evict the
// oldest cached frame first to make room for this one.
bool ImageCache::cacheImage(const String& timestamp) {
    if (LittleFS.usedBytes() + imageSize > LittleFS.totalBytes() * CACHE_FILL_THRESHOLD) {
        if (DEBUG_ENABLED) Serial.println("Cache full, evicting oldest frame...");
        cleanup();
    }

    if (!imageBuffer || imageSize == 0) {
        if (DEBUG_ENABLED) Serial.println("Invalid image buffer");
        return false;
    }

    String path = getCachePath(timestamp);
    File file = LittleFS.open(path, "w", true);
    if (!file) {
        if (DEBUG_ENABLED) { Serial.print("Cannot open for write: "); Serial.println(path); }
        return false;
    }

    size_t written = file.write(imageBuffer, imageSize);
    file.close();

    if (written != imageSize) {
        if (DEBUG_ENABLED) {
            Serial.printf("Write incomplete: %d of %d bytes\n", written, imageSize);
        }
        return false;
    }

    entries[writeIndex] = {timestamp, true};
    writeIndex = (writeIndex + 1) % CACHE_SIZE;

    if (DEBUG_ENABLED) Serial.printf("Cached %s (%d bytes)\n", timestamp.c_str(), written);
    return true;
}

// Load a cached JPEG into imageBuffer / imageSize.
// Deletes the file and returns false if it exists but is zero-length (corrupt).
bool ImageCache::loadImage(const String& timestamp) {
    String path = getCachePath(timestamp);
    if (!LittleFS.exists(path)) return false;

    File file = LittleFS.open(path, "r");
    if (!file) return false;

    imageSize = file.size();
    if (imageSize == 0) {
        file.close();
        LittleFS.remove(path);  // remove corrupt empty file
        return false;
    }

    if (imageBuffer) free(imageBuffer);
    imageBuffer = (uint8_t *)malloc(imageSize);
    if (!imageBuffer) {
        file.close();
        return false;
    }

    file.read(imageBuffer, imageSize);
    file.close();
    return true;
}

// Evict exactly enough old frames to make room for one incoming image.
// Each pass does a full directory scan to find the lexicographically smallest
// filename (= chronologically oldest frame) and removes it. Continues until
// free space exceeds imageSize.
void ImageCache::cleanup() {
    if (DEBUG_ENABLED) {
        Serial.printf("Cache cleanup — used before: %d bytes\n", LittleFS.usedBytes());
    }

    size_t targetUsage = LittleFS.totalBytes() - imageSize;
    int    removed     = 0;

    while (LittleFS.usedBytes() > targetUsage) {
        // Scan /cache/ for the oldest (lexicographically smallest) filename.
        String oldestPath;
        File dir = LittleFS.open("/cache");
        if (!dir || !dir.isDirectory()) break;

        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                String fp = String(f.path());
                if (oldestPath.isEmpty() || fp < oldestPath) oldestPath = fp;
            }
            f = dir.openNextFile();
        }
        dir.close();

        if (oldestPath.isEmpty()) break;

        if (LittleFS.remove(oldestPath)) {
            removed++;
            if (DEBUG_ENABLED) Serial.printf("Evicted: %s\n", oldestPath.c_str());
        } else {
            break;  // stop if a removal fails to avoid an infinite loop
        }
    }

    // Invalidate the in-memory ring buffer so subsequent loadImage() calls
    // consult the filesystem rather than stale metadata.
    for (int i = 0; i < CACHE_SIZE; i++) entries[i].valid = false;
    writeIndex = 0;

    if (DEBUG_ENABLED) {
        Serial.printf("Cache cleanup done — removed %d file(s), used after: %d bytes\n",
                      removed, LittleFS.usedBytes());
    }
}

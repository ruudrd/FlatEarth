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

    purgeStaleSatelliteCache();

    for (int i = 0; i < CACHE_SIZE; i++) entries[i].valid = false;

    if (DEBUG_ENABLED) Serial.println("Cache system initialized successfully");
    return true;
}

// Delete all cached frames from satellite directories other than the active one.
// Re-opens the directory on every iteration to avoid iterator invalidation when
// files are removed. Runs once at boot — cost is proportional to stale file count.
void ImageCache::purgeStaleSatelliteCache() {
    // Remove named satellite subdirectories that don't match the active SATTYPE.
    const char* allSats[] = {"GOES_EAST", "GOES_WEST", "ElektroL", "Meteosat", "MeteosatIODC"};
    for (const char* sat : allSats) {
        if (strcmp(sat, SATTYPE_NAME) == 0) continue;
        String dirPath = "/cache/" + String(sat);
        if (!LittleFS.exists(dirPath)) continue;

        int removed = 0;
        bool found = true;
        while (found) {
            found = false;
            File d = LittleFS.open(dirPath);
            if (!d || !d.isDirectory()) break;
            File f = d.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    String fp = String(f.path());
                    f.close();
                    d.close();
                    LittleFS.remove(fp);
                    removed++;
                    found = true;
                    break;
                }
                f = d.openNextFile();
            }
            if (d) d.close();
        }
        LittleFS.rmdir(dirPath);
        if (DEBUG_ENABLED)
            Serial.printf("Purged stale %s cache: %d files removed\n", sat, removed);
    }

    // Remove legacy flat files written directly into /cache/ by older firmware
    // (before satellite-namespaced subdirectories were introduced).
    int legacy = 0;
    bool found = true;
    while (found) {
        found = false;
        File root = LittleFS.open("/cache");
        if (!root || !root.isDirectory()) break;
        File f = root.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                String fp = String(f.path());
                f.close();
                root.close();
                LittleFS.remove(fp);
                legacy++;
                found = true;
                break;
            }
            f = root.openNextFile();
        }
        if (root) root.close();
    }
    if (DEBUG_ENABLED && legacy > 0)
        Serial.printf("Purged %d legacy flat cache files\n", legacy);
}

// Return the LittleFS path for a given timestamp under the active satellite's
// subdirectory, e.g. /cache/GOES_EAST/20261081300.jpg. Creates the directory
// hierarchy on first use so callers never need to worry about it.
String ImageCache::getCachePath(const String& timestamp) {
    if (!LittleFS.exists("/cache"))                        LittleFS.mkdir("/cache");
    String dir = "/cache/" + String(SATTYPE_NAME);
    if (!LittleFS.exists(dir))                             LittleFS.mkdir(dir);
    return dir + "/" + timestamp + ".jpg";
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

// Evict enough old frames to make room for one incoming image.
// Scans ALL satellite subdirectories under /cache/ so that switching satellites
// (or upgrading from the old flat /cache/<timestamp>.jpg layout) never leaves
// stale files from another source blocking eviction. The oldest file by filename
// across the whole tree is removed each pass.
void ImageCache::cleanup() {
    if (DEBUG_ENABLED) {
        Serial.printf("Cache cleanup — used before: %d bytes\n", LittleFS.usedBytes());
    }

    size_t targetUsage = LittleFS.totalBytes() - imageSize;
    int    removed     = 0;

    while (LittleFS.usedBytes() > targetUsage) {
        // Walk /cache/ and every subdirectory inside it, finding the file with
        // the lexicographically smallest name (= chronologically oldest frame).
        String oldestPath;
        String oldestName;

        File root = LittleFS.open("/cache");
        if (!root || !root.isDirectory()) break;

        File entry = root.openNextFile();
        while (entry) {
            if (entry.isDirectory()) {
                // Satellite subdirectory — scan its files.
                File f = entry.openNextFile();
                while (f) {
                    if (!f.isDirectory()) {
                        String name = String(f.name());
                        if (oldestName.isEmpty() || name < oldestName) {
                            oldestName = name;
                            oldestPath = String(f.path());
                        }
                    }
                    f = entry.openNextFile();
                }
            } else {
                // Legacy flat file directly in /cache/ (pre-namespacing format).
                String name = String(entry.name());
                if (oldestName.isEmpty() || name < oldestName) {
                    oldestName = name;
                    oldestPath = String(entry.path());
                }
            }
            entry = root.openNextFile();
        }
        root.close();

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

// Scan /cache/, count files and their total size, then print a summary with a
// suggestion so the user knows whether JPEG_QUALITY or NROFIMAGESTOSHOW can be tuned.
void ImageCache::printStats() {
    int    fileCount = 0;
    size_t dataBytes = 0;  // sum of actual JPEG sizes (not filesystem overhead)

    File dir = LittleFS.open("/cache/" + String(SATTYPE_NAME));
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                fileCount++;
                dataBytes += f.size();
            }
            f = dir.openNextFile();
        }
        dir.close();
    }

    size_t fsTotal = LittleFS.totalBytes();
    size_t fsUsed  = LittleFS.usedBytes();
    size_t fsFree  = fsTotal - fsUsed;
    float  fillPct = 100.0f * fsUsed / fsTotal;
    size_t avgSize = fileCount > 0 ? dataBytes / fileCount : 0;
    int    maxFrames = avgSize > 0 ? (int)(fsTotal * CACHE_FILL_THRESHOLD / avgSize) : 0;

    Serial.println(F("\n=== Cache stats ==="));
    Serial.printf("  Frames cached : %d\n",          fileCount);
    Serial.printf("  Avg frame size: %d bytes\n",     avgSize);
    Serial.printf("  Max frames fit: ~%d\n",          maxFrames);
    Serial.printf("  LittleFS used : %d KB / %d KB (%.1f%% full, %d KB free)\n",
                  fsUsed / 1024, fsTotal / 1024, fillPct, fsFree / 1024);

    // Give an actionable suggestion based on how full the cache is.
    if (fillPct < 60.0f) {
        Serial.println(F("  Tip: Cache has plenty of room."));
        Serial.printf( "       Try raising JPEG_QUALITY above %d, or increasing NROFIMAGESTOSHOW.\n", JPEG_QUALITY);
    } else if (fillPct < 85.0f) {
        Serial.println(F("  Tip: Cache usage is healthy — no changes needed."));
    } else if (fillPct < 95.0f) {
        Serial.println(F("  Tip: Cache is getting full."));
        Serial.printf( "       Consider lowering JPEG_QUALITY below %d, or reducing NROFIMAGESTOSHOW.\n", JPEG_QUALITY);
    } else {
        Serial.println(F("  Tip: Cache is nearly full — evictions are likely every cycle."));
        Serial.printf( "       Lower JPEG_QUALITY below %d or reduce NROFIMAGESTOSHOW.\n", JPEG_QUALITY);
    }
    Serial.println(F("===================\n"));
}

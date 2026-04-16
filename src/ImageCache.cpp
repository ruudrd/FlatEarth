#include "ImageCache.h"
#include "config.h"

// External references to the image buffer and size variables defined in main
extern uint8_t *imageBuffer;
extern size_t imageSize;

// Global cache instance
ImageCache cache;

/**
 * Initialize the cache system
 * @return true if LittleFS mounted successfully, false otherwise
 */
bool ImageCache::begin() {
    if (DEBUG_ENABLED) Serial.println("Initializing cache system...");
    
    // Format LittleFS if mounting fails
    if(!LittleFS.begin(true)) {
        if (DEBUG_ENABLED) Serial.println("LittleFS mount failed, attempting format...");
        if(!LittleFS.format()) {
            if (DEBUG_ENABLED) Serial.println("LittleFS format failed");
            return false;
        }
        if(!LittleFS.begin()) {
            if (DEBUG_ENABLED) Serial.println("LittleFS mount after format failed");
            return false;
        }
    }

    // Print LittleFS information
    if (DEBUG_ENABLED) {
        Serial.printf("Total LittleFS space: %d bytes\n", LittleFS.totalBytes());
        Serial.printf("Used LittleFS space: %d bytes\n", LittleFS.usedBytes());
        Serial.printf("Free LittleFS space: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
    }

    // Initialize cache entries
    for(int i = 0; i < CACHE_SIZE; i++) {
        entries[i].valid = false;
    }

    if (DEBUG_ENABLED) Serial.println("Cache system initialized successfully");
    return true;
}



/**
 * Generate the filesystem path for a cached image
 * @param timestamp The image timestamp to generate path for
 * @return Full path to the cached image file
 */
String ImageCache::getCachePath(const String& timestamp) {
    // Ensure directory exists before each write
    if (!LittleFS.exists("/cache")) {
        LittleFS.mkdir("/cache");
    }
    return "/cache/" + timestamp + ".jpg";
}

/**
 * Save current image buffer to cache
 * @param timestamp The timestamp to associate with this image
 * @return true if image was cached successfully, false otherwise
 */
bool ImageCache::cacheImage(const String& timestamp) {
    //Check available space
    if (LittleFS.usedBytes() + imageSize > LittleFS.totalBytes() * 0.9) { // Leave 10% free
        if (DEBUG_ENABLED) {
            Serial.println("LittleFS running low on space, cleaning old files...");
        }
        cleanup();  // Remove old files
    }

    if (imageBuffer == nullptr || imageSize == 0) {
        if (DEBUG_ENABLED) Serial.println("Invalid image buffer or size");
        return false;
    }
    
    String path = getCachePath(timestamp);
    File file = LittleFS.open(path, "w",true);
    if (!file) {
        if (DEBUG_ENABLED) {
            Serial.print("Failed to open file for writing: ");
            Serial.println(path);
        }
        return false;
    }
    
    size_t written = file.write(imageBuffer, imageSize);
    file.close();
    
    if (written != imageSize) {
        if (DEBUG_ENABLED) {
            Serial.printf("Write failed: %d of %d bytes written\n", written, imageSize);
        }
        return false;
    }
    
    entries[writeIndex] = {timestamp, true};
    writeIndex = (writeIndex + 1) % CACHE_SIZE;
    
    if (DEBUG_ENABLED) {
        Serial.printf("Cached image %s (%d bytes)\n", timestamp.c_str(), written);
    }
    return true;
}

/**
 * Load an image from cache into the image buffer
 * @param timestamp The timestamp of the image to load
 * @return true if image was loaded successfully, false if not found or error
 */
bool ImageCache::loadImage(const String &timestamp)
{
    // Check if file exists in cache
    String path = getCachePath(timestamp);
    if (!LittleFS.exists(path))
        return false;

    // Open cached file
    File file = LittleFS.open(path, "r");
    if (!file)
        return false;

    // Allocate buffer for image data
    imageSize = file.size();
    if (imageSize == 0) {
        file.close();
        LittleFS.remove(path);  // Remove corrupt empty file
        return false;
    }
    if (imageBuffer)
        free(imageBuffer);
    imageBuffer = (uint8_t *)malloc(imageSize);

    // Verify allocation succeeded
    if (!imageBuffer)
    {
        file.close();
        return false;
    }

    // Read image data and close file
    file.read(imageBuffer, imageSize);
    file.close();
    return true;
}

/**
 * Remove the oldest cached images from storage to free space
 * Scans LittleFS directly so it works correctly across reboots
 */
void ImageCache::cleanup() {
    if (DEBUG_ENABLED) {
        Serial.println("Starting cache cleanup...");
        Serial.printf("Before cleanup - Used space: %d bytes\n", LittleFS.usedBytes());

        File dir = LittleFS.open("/cache");
        if (dir && dir.isDirectory()) {
            File f = dir.openNextFile();
            while (f) {
                Serial.printf("Found file: %s, size: %d\n", f.path(), f.size());
                f = dir.openNextFile();
            }
            dir.close();
        }
    }

    // Remove oldest files until LittleFS usage is below 50% (or no more files)
    // Each pass scans for the lexicographically smallest (= chronologically oldest) file
    int filesRemoved = 0;
    size_t targetUsage = LittleFS.totalBytes() / 2;

    while (LittleFS.usedBytes() > targetUsage) {
        String oldestPath;
        File dir = LittleFS.open("/cache");
        if (!dir || !dir.isDirectory()) break;
        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                String fp = String(f.path());
                if (oldestPath.isEmpty() || fp < oldestPath) {
                    oldestPath = fp;
                }
            }
            f = dir.openNextFile();
        }
        dir.close();

        if (oldestPath.isEmpty()) break;

        if (LittleFS.remove(oldestPath)) {
            filesRemoved++;
            if (DEBUG_ENABLED) Serial.printf("Removed: %s\n", oldestPath.c_str());
        } else {
            break;
        }
    }

    // Invalidate in-memory entries
    for (int i = 0; i < CACHE_SIZE; i++) entries[i].valid = false;
    writeIndex = 0;

    if (DEBUG_ENABLED) {
        Serial.printf("Cleanup complete - Removed %d files\n", filesRemoved);
        Serial.printf("After cleanup - Used space: %d bytes\n", LittleFS.usedBytes());
    }
}
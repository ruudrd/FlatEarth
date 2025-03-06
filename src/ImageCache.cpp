#include "ImageCache.h"
#include "config.h"

// External references to the image buffer and size variables defined in main
extern uint8_t *imageBuffer;
extern size_t imageSize;

// Global cache instance
ImageCache cache;

/**
 * Initialize the cache system
 * @return true if SPIFFS mounted successfully, false otherwise
 */
bool ImageCache::begin() {
    if (DEBUG_ENABLED) Serial.println("Initializing cache system...");
    
    // Format SPIFFS if mounting fails
    if(!SPIFFS.begin(true)) {
        if (DEBUG_ENABLED) Serial.println("SPIFFS mount failed, attempting format...");
        if(!SPIFFS.format()) {
            if (DEBUG_ENABLED) Serial.println("SPIFFS format failed");
            return false;
        }
        if(!SPIFFS.begin()) {
            if (DEBUG_ENABLED) Serial.println("SPIFFS mount after format failed");
            return false;
        }
    }

    // Print SPIFFS information
    if (DEBUG_ENABLED) {
        Serial.printf("Total SPIFFS space: %d bytes\n", SPIFFS.totalBytes());
        Serial.printf("Used SPIFFS space: %d bytes\n", SPIFFS.usedBytes());
        Serial.printf("Free SPIFFS space: %d bytes\n", SPIFFS.totalBytes() - SPIFFS.usedBytes());
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
    if (!SPIFFS.exists("/cache")) {
        SPIFFS.mkdir("/cache");
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
    if (SPIFFS.usedBytes() + imageSize > SPIFFS.totalBytes() * 0.9) { // Leave 10% free
        if (DEBUG_ENABLED) {
            Serial.println("SPIFFS running low on space, cleaning old files...");
        }
        cleanup();  // Remove old files
    }

    if (imageBuffer == nullptr || imageSize == 0) {
        if (DEBUG_ENABLED) Serial.println("Invalid image buffer or size");
        return false;
    }
    
    String path = getCachePath(timestamp);
    File file = SPIFFS.open(path, "w",true);
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
    writeIndex = (writeIndex + 1) % NROFIMAGESTOSHOW;
    
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
    if (!SPIFFS.exists(path))
        return false;

    // Open cached file
    File file = SPIFFS.open(path, "r");
    if (!file)
        return false;

    // Allocate buffer for image data
    imageSize = file.size();
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
 * Remove all cached images from storage
 * Called when cache needs to be cleared
 */
void ImageCache::cleanup() {
    if (DEBUG_ENABLED) {
        Serial.println("Starting cache cleanup...");
        Serial.printf("Before cleanup - Used space: %d bytes\n", SPIFFS.usedBytes());
    }

    // First list all files in SPIFFS for debugging
    if (DEBUG_ENABLED) {
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        while(file) {
            Serial.printf("Found file: %s, size: %d\n", file.path(), file.size());
        #ifdef DELETE_ALL
            SPIFFS.remove(file.path());    // Remove all files from root
        #endif
            file = root.openNextFile();
        }
    }

    // Remove files and invalidate entries
    int filesRemoved = 0;
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (!entries[i].valid) continue;
        
        String path = getCachePath(entries[i].timestamp);
        if (SPIFFS.exists(path)) {
            if (SPIFFS.remove(path)) {
                filesRemoved++;
                if (DEBUG_ENABLED) {
                    Serial.printf("Removed file: %s\n", path.c_str());
                }
            } else {
                if (DEBUG_ENABLED) {
                    Serial.printf("Failed to remove file: %s\n", path.c_str());
                }
            }
        }
        entries[i].valid = false;  // Mark entry as invalid
    }

    // Reset write index
    writeIndex = 0;

    if (DEBUG_ENABLED) {
        Serial.printf("Cleanup complete - Removed %d files\n", filesRemoved);
        Serial.printf("After cleanup - Used space: %d bytes\n", SPIFFS.usedBytes());
    }
}
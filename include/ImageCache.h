#ifndef IMAGE_CACHE_H
#define IMAGE_CACHE_H

#include <Arduino.h>
#include <SPIFFS.h>
#include "config.h"

class ImageCache {
private:
    struct CacheEntry {
        String timestamp;
        bool valid;
    };
    
    CacheEntry entries[CACHE_SIZE];
    int writeIndex = 0;

public:
    bool begin();
    String getCachePath(const String& timestamp);
    bool cacheImage(const String& timestamp);
    bool loadImage(const String& timestamp);
    void cleanup();
};

extern ImageCache cache;

#endif
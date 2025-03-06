#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ImageCache.h>

class ImageDownloader {
public:
    static bool downloadImage(String timestamp);
    static void showLastXHours();
    static String getFormattedTime();

private:
    static String constructUrl(String timestamp);
};

extern uint8_t* imageBuffer;
extern size_t imageSize;

#endif
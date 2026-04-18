// WiFiManager.cpp — WiFi connection with dual-network fallback.

#include "WiFiManager.h"
#include <WiFi.h>
#include "config.h"

// Attempt to connect to a single WiFi network.
// Polls every WIFI_CONNECT_DELAY_MS ms up to WIFI_CONNECT_ATTEMPTS times.
// Returns true if the connection was established within the attempt limit.
static bool tryConnect(const char *ssid, const char *password) {
    WiFi.begin(ssid, password);
    for (int i = 0; i < WIFI_CONNECT_ATTEMPTS; i++) {
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(WIFI_CONNECT_DELAY_MS);
        if (DEBUG_ENABLED) Serial.print(".");
    }
    return WiFi.status() == WL_CONNECTED;
}

void setupWiFi() {
    WiFi.setHostname(DEVICENAME);

    if (DEBUG_ENABLED) Serial.println("Connecting to primary WiFi...");
    if (!tryConnect(WIFI_SSID1, WIFI_PASSWORD1)) {
        if (DEBUG_ENABLED) Serial.println("\nTrying backup WiFi...");
        tryConnect(WIFI_SSID2, WIFI_PASSWORD2);
    }

    if (DEBUG_ENABLED) {
        if (WiFi.status() == WL_CONNECTED)
            Serial.println("\nWiFi connected!");
        else
            Serial.println("\nWiFi connection failed.");
    }
}

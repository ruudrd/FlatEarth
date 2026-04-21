// WiFiManager.cpp — WiFi connection with dual-network fallback.

#include "WiFiManager.h"
#include <WiFi.h>
#include "config.h"
#include "Display.h"

// Attempt to connect to one WiFi network, showing the SSID on screen.
// Disconnects any in-progress connection first to avoid ESP_ERR_WIFI_CONN errors.
// Polls every WIFI_CONNECT_DELAY_MS ms up to WIFI_CONNECT_ATTEMPTS times.
// Shows "WiFi OK" in green and returns true on success.
// Returns false on timeout without showing a failure message — the caller decides
// whether to try a backup or show the final error, so no premature red text appears.
static bool tryConnect(const char *ssid, const char *password) {
    char msg[40];
    snprintf(msg, sizeof(msg), "WiFi: %.28s", ssid);
    showStatus(msg);
    if (DEBUG_ENABLED) { Serial.print("Connecting to "); Serial.println(ssid); }

    WiFi.disconnect(true);  // stop any in-progress attempt before starting a new one
    delay(100);
    WiFi.begin(ssid, password);

    for (int i = 0; i < WIFI_CONNECT_ATTEMPTS; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            showStatus("WiFi OK", 0x07E0);
            if (DEBUG_ENABLED) Serial.println("WiFi connected!");
            return true;
        }
        delay(WIFI_CONNECT_DELAY_MS);
        if (DEBUG_ENABLED) Serial.print(".");
    }
    if (DEBUG_ENABLED) Serial.println();
    return false;
}

void setupWiFi() {
    WiFi.setHostname(DEVICENAME);
    if (!tryConnect(WIFI_SSID1, WIFI_PASSWORD1)) {
        if (!tryConnect(WIFI_SSID2, WIFI_PASSWORD2)) {
            showStatus("No WiFi!", 0xF800);
            if (DEBUG_ENABLED) Serial.println("Both networks failed.");
        }
    }
}

// WiFiManager.h — WiFi connection with dual-network fallback.

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// Connect to WiFi using the credentials defined in secrets.h.
// Tries the primary network (WIFI_SSID1) first. If it cannot connect within
// WIFI_CONNECT_ATTEMPTS × WIFI_CONNECT_DELAY_MS milliseconds, it falls back
// to the secondary network (WIFI_SSID2).
// Blocks until a connection is established or both networks fail.
void setupWiFi();

#endif

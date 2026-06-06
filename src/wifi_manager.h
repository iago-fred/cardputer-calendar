#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <vector>

struct ScannedNetwork {
    String ssid;
    int32_t rssi;
    uint8_t encryption;
};

class WiFiManager {
public:
    // Try each saved network; returns true if connected
    bool connectFromSaved();

    // Scan visible networks (async-safe, blocking = wait for results)
    bool scan(std::vector<ScannedNetwork>& results, bool blocking = true);

    // Connect to a specific network
    bool connect(const String& ssid, const String& password, uint32_t timeoutMs = 10000);

    // Disconnect
    void disconnect();

    bool isConnected() { return _connected; }
    String currentSSID() { return _current_ssid; }
    int32_t currentRSSI();

private:
    bool _connected = false;
    String _current_ssid;
};

extern WiFiManager wifiMgr;

#endif

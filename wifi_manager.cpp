#include "wifi_manager.h"
#include "storage.h"
#include <WiFi.h>

WiFiManager wifiMgr;

bool WiFiManager::connectFromSaved() {
    std::vector<WifiNetwork> nets;
    if (!storage.loadWifiNetworks(nets) || nets.empty()) {
        log_w("No saved WiFi networks");
        return false;
    }
    for (auto& n : nets) {
        log_i("Trying %s...", n.ssid.c_str());
        if (connect(n.ssid, n.password, 8000)) {
            return true;
        }
    }
    log_e("All saved networks failed");
    return false;
}

bool WiFiManager::scan(std::vector<ScannedNetwork>& results, bool blocking) {
    results.clear();
    // Ensure WiFi is in station mode before scanning
    WiFi.mode(WIFI_STA);
    delay(200);

    int n;
    if (blocking) {
        n = WiFi.scanNetworks();
        while (n == WIFI_SCAN_RUNNING) { delay(50); n = WiFi.scanComplete(); }
    } else {
        n = WiFi.scanNetworks();
    }
    if (n < 0) {
        log_e("WiFi scan failed: %d", n);
        return false;
    }
    // De-duplicate by SSID (keep strongest)
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        bool dup = false;
        for (auto& r : results) {
            if (r.ssid == ssid) { dup = true; break; }
        }
        if (!dup) {
            results.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i)});
        }
    }
    // Sort by RSSI descending
    std::sort(results.begin(), results.end(),
        [](const ScannedNetwork& a, const ScannedNetwork& b) { return a.rssi > b.rssi; });
    WiFi.scanDelete();
    return true;
}

bool WiFiManager::connect(const String& ssid, const String& password, uint32_t timeoutMs) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        _connected   = true;
        _current_ssid = ssid;
        log_i("Connected to %s, IP: %s", ssid.c_str(), WiFi.localIP().toString().c_str());
        return true;
    }

    log_w("Failed to connect to %s (timeout %dms)", ssid.c_str(), timeoutMs);
    _connected = false;
    return false;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    _connected = false;
    _current_ssid = "";
}

int32_t WiFiManager::currentRSSI() {
    if (!_connected) return -999;
    return WiFi.RSSI();
}

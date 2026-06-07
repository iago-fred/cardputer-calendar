#include "storage.h"
#include <algorithm>
#include <SPI.h>

StorageManager storage;

bool StorageManager::begin() {
    // M5Cardputer V1.1 TF card pins (from official docs):
    // CS=G12, MOSI=G14, CLK=G40, MISO=G39
    // No conflict with keyboard (GPIO {8,9,11,13,15,3,4,5,6,7})
    SPI.begin(40, 39, 14);  // SCK, MISO, MOSI
    pinMode(12, OUTPUT);
    if (!SD.begin(12)) {
        log_e("SD card mount failed (CS=12)");
        _ready = false;
        return false;
    }
    _ready = true;
    log_i("SD card mounted successfully");
    return true;
}

// ─── Auth ───
bool StorageManager::loadAuth(OAuthConfig& cfg) {
    if (!fileExists(AUTH_FILE)) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, readFile(AUTH_FILE));
    if (err) { log_e("Auth JSON parse: %s", err.c_str()); return false; }
    cfg.client_id     = doc["client_id"]     | "";
    cfg.client_secret = doc["client_secret"] | "";
    cfg.refresh_token = doc["refresh_token"] | "";
    return cfg.client_id.length() > 0 && cfg.refresh_token.length() > 0;
}

bool StorageManager::saveAuth(const OAuthConfig& cfg) {
    JsonDocument doc;
    doc["client_id"]     = cfg.client_id;
    doc["client_secret"] = cfg.client_secret;
    doc["refresh_token"] = cfg.refresh_token;
    String out;
    serializeJsonPretty(doc, out);
    return writeFile(AUTH_FILE, out);
}

// ─── WiFi ───
bool StorageManager::loadWifiNetworks(std::vector<WifiNetwork>& nets) {
    nets.clear();
    if (!fileExists(WIFI_FILE)) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, readFile(WIFI_FILE));
    if (err) { log_e("WiFi JSON parse: %s", err.c_str()); return false; }
    JsonArray arr = doc["networks"].as<JsonArray>();
    for (auto n : arr) {
        WifiNetwork w;
        w.ssid     = n["ssid"]     | "";
        w.password = n["password"] | "";
        if (w.ssid.length() > 0) nets.push_back(w);
    }
    return true;
}

bool StorageManager::saveWifiNetworks(const std::vector<WifiNetwork>& nets) {
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (auto& n : nets) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"]     = n.ssid;
        o["password"] = n.password;
    }
    String out;
    serializeJsonPretty(doc, out);
    return writeFile(WIFI_FILE, out);
}

bool StorageManager::addWifiNetwork(const WifiNetwork& net) {
    std::vector<WifiNetwork> nets;
    loadWifiNetworks(nets);
    // Replace if exists, else append
    for (auto& n : nets) {
        if (n.ssid == net.ssid) { n.password = net.password; return saveWifiNetworks(nets); }
    }
    nets.push_back(net);
    return saveWifiNetworks(nets);
}

bool StorageManager::removeWifiNetwork(const String& ssid) {
    std::vector<WifiNetwork> nets;
    loadWifiNetworks(nets);
    auto it = std::remove_if(nets.begin(), nets.end(), [&](const WifiNetwork& n) { return n.ssid == ssid; });
    if (it == nets.end()) return false;
    nets.erase(it, nets.end());
    return saveWifiNetworks(nets);
}

// ─── Events Cache ───
bool StorageManager::loadEvents(std::vector<CalendarEvent>& events) {
    events.clear();
    if (!fileExists(EVENTS_CACHE_FILE)) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, readFile(EVENTS_CACHE_FILE));
    if (err) { log_e("Events JSON parse: %s", err.c_str()); return false; }
    JsonArray arr = doc["events"].as<JsonArray>();
    for (auto e : arr) {
        CalendarEvent ev;
        ev.id           = e["id"]           | "";
        ev.summary      = e["summary"]      | "";
        ev.description  = e["description"]  | "";
        ev.start_date   = e["start_date"]   | "";
        ev.end_date     = e["end_date"]     | "";
        ev.is_all_day   = e["is_all_day"]   | false;
        ev.color_id     = e["color_id"]     | "";
        ev.updated_ms   = e["updated_ms"]   | (int64_t)0;
        ev.deleted      = e["deleted"]      | false;
        ev.dirty        = e["dirty"]        | false;
        events.push_back(ev);
    }
    return true;
}

bool StorageManager::saveEvents(const std::vector<CalendarEvent>& events) {
    JsonDocument doc;
    JsonArray arr = doc["events"].to<JsonArray>();
    for (auto& ev : events) {
        JsonObject o = arr.add<JsonObject>();
        o["id"]          = ev.id;
        o["summary"]     = ev.summary;
        o["description"] = ev.description;
        o["start_date"]  = ev.start_date;
        o["end_date"]    = ev.end_date;
        o["is_all_day"]  = ev.is_all_day;
        o["color_id"]    = ev.color_id;
        o["updated_ms"]  = ev.updated_ms;
        o["deleted"]     = ev.deleted;
        o["dirty"]       = ev.dirty;
    }
    String out;
    serializeJsonPretty(doc, out);
    return writeFile(EVENTS_CACHE_FILE, out);
}

bool StorageManager::updateEventInCache(const CalendarEvent& evt) {
    std::vector<CalendarEvent> events;
    loadEvents(events);
    for (auto& e : events) {
        if (e.id == evt.id) { e = evt; return saveEvents(events); }
    }
    events.push_back(evt);
    return saveEvents(events);
}

bool StorageManager::deleteEventFromCache(const String& id) {
    std::vector<CalendarEvent> events;
    loadEvents(events);
    auto it = std::remove_if(events.begin(), events.end(), [&](const CalendarEvent& e) { return e.id == id; });
    if (it == events.end()) return false;
    events.erase(it, events.end());
    return saveEvents(events);
}

// ─── Pending Changes ───
bool StorageManager::loadPending(std::vector<PendingChange>& pending) {
    pending.clear();
    if (!fileExists(PENDING_CHANGES_FILE)) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, readFile(PENDING_CHANGES_FILE));
    if (err) { log_e("Pending JSON parse: %s", err.c_str()); return false; }
    JsonArray arr = doc["pending"].as<JsonArray>();
    for (auto p : arr) {
        PendingChange ch;
        ch.action = p["action"] | "";
        auto e = p["event"];
        ch.event.id           = e["id"]           | "";
        ch.event.summary      = e["summary"]      | "";
        ch.event.description  = e["description"]  | "";
        ch.event.start_date   = e["start_date"]   | "";
        ch.event.end_date     = e["end_date"]     | "";
        ch.event.is_all_day   = e["is_all_day"]   | false;
        ch.event.color_id     = e["color_id"]     | "";
        ch.event.updated_ms   = e["updated_ms"]   | (int64_t)0;
        ch.event.deleted      = e["deleted"]      | false;
        ch.event.dirty        = e["dirty"]        | false;
        if (ch.action.length() > 0) pending.push_back(ch);
    }
    return true;
}

bool StorageManager::savePending(const std::vector<PendingChange>& pending) {
    JsonDocument doc;
    JsonArray arr = doc["pending"].to<JsonArray>();
    for (auto& p : pending) {
        JsonObject o = arr.add<JsonObject>();
        o["action"] = p.action;
        JsonObject e = o["event"].to<JsonObject>();
        e["id"]          = p.event.id;
        e["summary"]     = p.event.summary;
        e["description"] = p.event.description;
        e["start_date"]  = p.event.start_date;
        e["end_date"]    = p.event.end_date;
        e["is_all_day"]  = p.event.is_all_day;
        e["color_id"]    = p.event.color_id;
        e["updated_ms"]  = p.event.updated_ms;
        e["deleted"]     = p.event.deleted;
        e["dirty"]       = p.event.dirty;
    }
    String out;
    serializeJsonPretty(doc, out);
    return writeFile(PENDING_CHANGES_FILE, out);
}

bool StorageManager::addPendingChange(const PendingChange& change) {
    std::vector<PendingChange> pending;
    loadPending(pending);
    pending.push_back(change);
    return savePending(pending);
}

bool StorageManager::clearPending() {
    return writeFile(PENDING_CHANGES_FILE, "{\"pending\":[]}");
}

// ─── Last Sync ───
bool StorageManager::loadLastSync(String& syncToken, int64_t& lastSyncMs) {
    if (!fileExists(LAST_SYNC_FILE)) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, readFile(LAST_SYNC_FILE));
    if (err) return false;
    syncToken  = doc["sync_token"] | "";
    lastSyncMs = doc["last_sync_ms"] | (int64_t)0;
    return true;
}

bool StorageManager::saveLastSync(const String& syncToken, int64_t lastSyncMs) {
    JsonDocument doc;
    doc["sync_token"]   = syncToken;
    doc["last_sync_ms"] = lastSyncMs;
    String out;
    serializeJsonPretty(doc, out);
    return writeFile(LAST_SYNC_FILE, out);
}

// ─── Utility ───
bool StorageManager::fileExists(const char* path) {
    return SD.exists(path);
}

String StorageManager::readFile(const char* path) {
    File f = SD.open(path, FILE_READ);
    if (!f) return "";
    String s;
    while (f.available()) s += (char)f.read();
    f.close();
    return s;
}

bool StorageManager::writeFile(const char* path, const String& content) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.print(content);
    f.close();
    return written > 0;
}

bool StorageManager::deleteFile(const char* path) {
    return SD.remove(path);
}

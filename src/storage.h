#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>

// ─── SD File Paths ───
#define AUTH_FILE           "/auth.json"
#define WIFI_FILE           "/wifi_config.json"
#define EVENTS_CACHE_FILE   "/events_cache.json"
#define PENDING_CHANGES_FILE "/pending_changes.json"
#define LAST_SYNC_FILE      "/last_sync.json"

// ─── Auth JSON ───
struct OAuthConfig {
    String client_id;
    String client_secret;
    String refresh_token;
};

// ─── WiFi Network ───
struct WifiNetwork {
    String ssid;
    String password;
};

// ─── Calendar Event ───
struct CalendarEvent {
    String id;           // empty = new (unsynced)
    String summary;
    String description;
    String start_date;   // YYYY-MM-DD or ISO datetime
    String end_date;
    bool   is_all_day;
    String color_id;     // Google Calendar colorId
    int64_t updated_ms;  // last modified epoch ms
    bool   deleted;      // soft-delete for sync
    bool   dirty;        // modified locally, needs sync
};

struct PendingChange {
    String action;       // "create" | "update" | "delete"
    CalendarEvent event;
};

// ─── Storage Manager ───
class StorageManager {
public:
    bool begin();
    bool isReady() { return _ready; }

    // Auth
    bool loadAuth(OAuthConfig& cfg);
    bool saveAuth(const OAuthConfig& cfg);

    // WiFi
    bool loadWifiNetworks(std::vector<WifiNetwork>& nets);
    bool saveWifiNetworks(const std::vector<WifiNetwork>& nets);
    bool addWifiNetwork(const WifiNetwork& net);
    bool removeWifiNetwork(const String& ssid);

    // Events cache (offline)
    bool loadEvents(std::vector<CalendarEvent>& events);
    bool saveEvents(const std::vector<CalendarEvent>& events);
    bool updateEventInCache(const CalendarEvent& evt);
    bool deleteEventFromCache(const String& id);

    // Pending changes
    bool loadPending(std::vector<PendingChange>& pending);
    bool savePending(const std::vector<PendingChange>& pending);
    bool addPendingChange(const PendingChange& change);
    bool clearPending();

    // Last sync info
    bool loadLastSync(String& syncToken, int64_t& lastSyncMs);
    bool saveLastSync(const String& syncToken, int64_t lastSyncMs);

    // Utility
    bool   fileExists(const char* path);
    String readFile(const char* path);
    bool   writeFile(const char* path, const String& content);
    bool   deleteFile(const char* path);

private:
    bool _ready = false;
};

extern StorageManager storage;

#endif

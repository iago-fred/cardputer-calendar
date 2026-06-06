#include "calendar_api.h"
#include "oauth2.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

CalendarAPI calendar;

static String urlEncode(const String& s) {
    String out;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += c;
        else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            out += buf;
        }
    }
    return out;
}

String CalendarAPI::getAccessToken() {
    String token;
    if (!oauth2.getAccessToken(token)) return "";
    return token;
}

// ─── Fetch events for a range ───
bool CalendarAPI::fetchEvents(const String& timeMin, const String& timeMax, std::vector<CalendarEvent>& events) {
    events.clear();
    String token = getAccessToken();
    if (token.length() == 0) return false;

    String url = String(CAL_BASE) + "/events?"
        "orderBy=startTime&singleEvents=true"
        "&timeMin=" + urlEncode(timeMin) +
        "&timeMax=" + urlEncode(timeMax) +
        "&maxResults=250";

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + token);

    // Pagination
    String pageToken;
    do {
        String fullUrl = url;
        if (pageToken.length() > 0) fullUrl += "&pageToken=" + urlEncode(pageToken);

        http.begin(client, fullUrl);
        http.addHeader("Authorization", "Bearer " + token);
        int code = http.GET();
        if (code != 200) {
            log_e("Calendar fetch: HTTP %d", code);
            log_e("Response: %s", http.getString().c_str());
            http.end();
            return false;
        }
        String resp = http.getString();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (err) { log_e("JSON parse: %s", err.c_str()); http.end(); return false; }

        JsonArray items = doc["items"].as<JsonArray>();
        for (auto item : items) {
            CalendarEvent ev = parseEventItem(item);
            events.push_back(ev);
        }
        pageToken = doc["nextPageToken"] | "";
        http.end();
    } while (pageToken.length() > 0);

    log_i("Fetched %d events [%s ~ %s]", events.size(), timeMin.c_str(), timeMax.c_str());
    return true;
}

// ─── Incremental sync ───
bool CalendarAPI::syncEvents(const String& syncToken, std::vector<CalendarEvent>& changed, String& newSyncToken) {
    changed.clear();
    String token = getAccessToken();
    if (token.length() == 0) return false;

    String url = String(CAL_BASE) + "/events?"
        "showDeleted=true&maxResults=250"
        "&syncToken=" + urlEncode(syncToken);

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String pageToken;
    do {
        String fullUrl = url;
        if (pageToken.length() > 0) fullUrl += "&pageToken=" + urlEncode(pageToken);

        http.begin(client, fullUrl);
        http.addHeader("Authorization", "Bearer " + token);
        int code = http.GET();

        // 410 means sync token expired → do full resync
        if (code == 410) {
            log_w("Sync token expired, need full resync");
            http.end();
            return false;
        }
        if (code != 200) {
            log_e("Calendar sync: HTTP %d", code);
            http.end();
            return false;
        }

        String resp = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (err) { log_e("Sync JSON parse: %s", err.c_str()); return false; }

        JsonArray items = doc["items"].as<JsonArray>();
        for (auto item : items) {
            CalendarEvent ev = parseEventItem(item);
            changed.push_back(ev);
        }
        newSyncToken = doc["nextSyncToken"] | "";
        pageToken    = doc["nextPageToken"] | "";
    } while (pageToken.length() > 0);

    log_i("Sync: %d changes, new sync token: %s", changed.size(), newSyncToken.c_str());
    return true;
}

// ─── Create Event ───
bool CalendarAPI::createEvent(const CalendarEvent& evt, CalendarEvent& created) {
    String token = getAccessToken();
    if (token.length() == 0) return false;

    JsonDocument body;
    body["summary"] = evt.summary;
    if (evt.description.length() > 0) body["description"] = evt.description;
    if (evt.color_id.length() > 0) body["colorId"] = evt.color_id;

    if (evt.is_all_day) {
        body["start"]["date"] = evt.start_date.substring(0, 10);
        body["end"]["date"]   = evt.end_date.substring(0, 10);
    } else {
        body["start"]["dateTime"] = evt.start_date;
        body["start"]["timeZone"] = "America/Bahia";
        body["end"]["dateTime"]   = evt.end_date;
        body["end"]["timeZone"]   = "America/Bahia";
    }

    String jsonBody;
    serializeJson(body, jsonBody);

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, String(CAL_BASE) + "/events");
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonBody);
    if (code != 200) {
        log_e("Create event: HTTP %d — %s", code, http.getString().c_str());
        http.end();
        return false;
    }
    String resp = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) { log_e("Create response parse: %s", err.c_str()); return false; }

    created = parseEventItem(doc.as<JsonObject>());
    log_i("Created event: %s (%s)", created.summary.c_str(), created.id.c_str());
    return true;
}

// ─── Update Event ───
bool CalendarAPI::updateEvent(const CalendarEvent& evt) {
    if (evt.id.length() == 0) return false;
    String token = getAccessToken();
    if (token.length() == 0) return false;

    JsonDocument body;
    body["summary"] = evt.summary;
    if (evt.description.length() > 0) body["description"] = evt.description;
    if (evt.color_id.length() > 0) body["colorId"] = evt.color_id;

    if (evt.is_all_day) {
        body["start"]["date"] = evt.start_date.substring(0, 10);
        body["end"]["date"]   = evt.end_date.substring(0, 10);
    } else {
        body["start"]["dateTime"] = evt.start_date;
        body["start"]["timeZone"] = "America/Bahia";
        body["end"]["dateTime"]   = evt.end_date;
        body["end"]["timeZone"]   = "America/Bahia";
    }

    String jsonBody;
    serializeJson(body, jsonBody);

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, String(CAL_BASE) + "/events/" + urlEncode(evt.id));
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    int code = http.PUT(jsonBody);
    if (code != 200) {
        log_e("Update event: HTTP %d — %s", code, http.getString().c_str());
        http.end();
        return false;
    }
    http.end();
    log_i("Updated event: %s", evt.summary.c_str());
    return true;
}

// ─── Delete Event ───
bool CalendarAPI::deleteEvent(const String& eventId) {
    if (eventId.length() == 0) return false;
    String token = getAccessToken();
    if (token.length() == 0) return false;

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, String(CAL_BASE) + "/events/" + urlEncode(eventId));
    http.addHeader("Authorization", "Bearer " + token);
    int code = http.sendRequest("DELETE");
    if (code != 204) {
        log_e("Delete event: HTTP %d — %s", code, http.getString().c_str());
        http.end();
        return false;
    }
    http.end();
    log_i("Deleted event: %s", eventId.c_str());
    return true;
}

// ─── Parse a single event from JSON ───
CalendarEvent CalendarAPI::parseEventItem(JsonObject& item) {
    CalendarEvent ev;
    ev.id         = item["id"]         | "";
    ev.summary    = item["summary"]    | "(sem título)";
    ev.description= item["description"]| "";
    ev.color_id   = item["colorId"]    | "";
    ev.deleted    = item["status"] == "cancelled" || false;
    ev.dirty      = false;

    // updated is RFC3339 — convert to epoch ms
    String updated = item["updated"] | "";
    if (updated.length() > 0) {
        // Parse simple: take the raw string as fallback
        // Full ISO parsing is complex; store for sync
    }

    // Start
    JsonObject start = item["start"];
    if (start["dateTime"].is<String>()) {
        ev.start_date = start["dateTime"] | "";
        ev.is_all_day = false;
    } else {
        ev.start_date = start["date"] | "";
        ev.is_all_day = true;
    }
    // End
    JsonObject end = item["end"];
    if (end["dateTime"].is<String>()) {
        ev.end_date = end["dateTime"] | "";
    } else {
        ev.end_date = end["date"] | "";
    }

    // Parse updated timestamp
    String updatedStr = item["updated"] | "";
    if (updatedStr.length() > 0) {
        struct tm tm;
        if (strptime(updatedStr.c_str(), "%Y-%m-%dT%H:%M:%S", &tm)) {
            time_t t = mktime(&tm);
            ev.updated_ms = (int64_t)t * 1000;
        }
    }

    return ev;
}

// ─── Date Helpers ───
String CalendarAPI::todayMin() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT00:00:00Z", t);
    return String(buf);
}

String CalendarAPI::todayMax() {
    time_t now = time(nullptr) + 86400;
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT00:00:00Z", t);
    return String(buf);
}

String CalendarAPI::nowISO() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", t);
    return String(buf);
}

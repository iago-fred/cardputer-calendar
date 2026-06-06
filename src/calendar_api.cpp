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

    String baseUrl = String(CAL_BASE) + "/events?"
        "orderBy=startTime&singleEvents=true"
        "&timeMin=" + urlEncode(timeMin) +
        "&timeMax=" + urlEncode(timeMax) +
        "&maxResults=250";

    WiFiClientSecure client;
    client.setInsecure();

    String pageToken;
    do {
        HTTPClient http;
        String fullUrl = baseUrl;
        if (pageToken.length() > 0) fullUrl += "&pageToken=" + urlEncode(pageToken);

        http.begin(client, fullUrl);
        http.addHeader("Authorization", "Bearer " + token);
        int code = http.GET();

        if (code != 200) {
            log_e("Calendar fetch: HTTP %d", code);
            http.end();
            return false;
        }
        String resp = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (err) { log_e("JSON parse: %s", err.c_str()); return false; }

        JsonArray items = doc["items"].as<JsonArray>();
        for (auto item : items) {
            events.push_back(parseEventItem(item));
        }
        pageToken = doc["nextPageToken"] | "";
    } while (pageToken.length() > 0);

    log_i("Fetched %d events", events.size());
    return true;
}

// ─── Incremental sync ───
bool CalendarAPI::syncEvents(const String& syncToken, std::vector<CalendarEvent>& changed, String& newSyncToken) {
    changed.clear();
    String token = getAccessToken();
    if (token.length() == 0) return false;

    String baseUrl = String(CAL_BASE) + "/events?"
        "showDeleted=true&maxResults=250"
        "&syncToken=" + urlEncode(syncToken);

    WiFiClientSecure client;
    client.setInsecure();

    String pageToken;
    do {
        HTTPClient http;
        String fullUrl = baseUrl;
        if (pageToken.length() > 0) fullUrl += "&pageToken=" + urlEncode(pageToken);

        http.begin(client, fullUrl);
        http.addHeader("Authorization", "Bearer " + token);
        int code = http.GET();

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
            changed.push_back(parseEventItem(item));
        }
        newSyncToken = doc["nextSyncToken"] | "";
        pageToken    = doc["nextPageToken"] | "";
    } while (pageToken.length() > 0);

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

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String(CAL_BASE) + "/events");
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonBody);
    if (code != 200) {
        log_e("Create event: HTTP %d", code);
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

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String(CAL_BASE) + "/events/" + urlEncode(evt.id));
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    int code = http.PUT(jsonBody);
    if (code != 200) {
        log_e("Update event: HTTP %d", code);
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

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String(CAL_BASE) + "/events/" + urlEncode(eventId));
    http.addHeader("Authorization", "Bearer " + token);
    int code = http.sendRequest("DELETE");
    if (code != 204) {
        log_e("Delete event: HTTP %d", code);
        http.end();
        return false;
    }
    http.end();
    log_i("Deleted event: %s", eventId.c_str());
    return true;
}

// ─── Convert RFC 3339 to epoch ms (manual, no strptime) ───
uint64_t CalendarAPI::parseRfc3339(const String& rfc3339) {
    // Expected: "2026-06-06T09:00:00.000Z" or "2026-06-06T09:00:00-03:00"
    int y = 0, M = 0, d = 0, h = 0, m = 0, s = 0;
    if (sscanf(rfc3339.c_str(), "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s) < 6) {
        return 0;
    }
    // Days from 1970-01-01 (simplified — doesn't account for leap seconds)
    // tm struct alternative
    struct tm tm = {0};
    tm.tm_year = y - 1900;
    tm.tm_mon  = M - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min  = m;
    tm.tm_sec  = s;
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    return (uint64_t)t * 1000;
}

// ─── Parse a single event from JSON ───
CalendarEvent CalendarAPI::parseEventItem(JsonObject& item) {
    CalendarEvent ev;
    ev.id         = item["id"]         | "";
    ev.summary    = item["summary"]    | "(sem titulo)";
    ev.description= item["description"]| "";
    ev.color_id   = item["colorId"]    | "";
    ev.deleted    = (item["status"] == "cancelled");
    ev.dirty      = false;

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
        ev.updated_ms = parseRfc3339(updatedStr);
    }

    return ev;
}

// ─── Date Helpers (UTC ISO strings, no "Z" suffix — use +00:00 for clarity) ───
String CalendarAPI::todayMin() {
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);  // gmtime, not localtime!
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT00:00:00Z", t);
    return String(buf);
}

String CalendarAPI::todayMax() {
    time_t now = time(nullptr) + 86400;
    struct tm* t = gmtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT00:00:00Z", t);
    return String(buf);
}

String CalendarAPI::nowISO() {
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
    return String(buf);
}

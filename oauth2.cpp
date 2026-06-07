#include "oauth2.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

OAuth2Manager oauth2;

void OAuth2Manager::setConfig(const String& clientId, const String& clientSecret, const String& refreshToken) {
    _client_id     = clientId;
    _client_secret = clientSecret;
    _refresh_token = refreshToken;
}

bool OAuth2Manager::getAccessToken(String& outToken) {
    // If we have a cached token with >60s remaining, reuse it
    if (_access.token.length() > 0 && _access.expires_in > 0) {
        uint64_t elapsed = (millis() - _access.fetched_at) / 1000;
        if (elapsed < (uint64_t)(_access.expires_in - 60)) {
            outToken = _access.token;
            return true;
        }
    }
    // Refresh
    if (!refreshAccessToken()) return false;
    outToken = _access.token;
    return true;
}

bool OAuth2Manager::refreshAccessToken() {
    if (!isConfigured()) {
        log_e("OAuth2 not configured");
        return false;
    }

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Google certs — we trust the Let's Encrypt chain

    http.begin(client, TOKEN_URL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "client_id="     + _client_id
                + "&client_secret=" + _client_secret
                + "&refresh_token=" + _refresh_token
                + "&grant_type=refresh_token";

    int code = http.POST(body);
    if (code != 200) {
        log_e("Token refresh failed: HTTP %d", code);
        log_e("Response: %s", http.getString().c_str());
        http.end();
        return false;
    }

    String resp = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        log_e("Token JSON parse: %s", err.c_str());
        return false;
    }

    _access.token      = doc["access_token"] | "";
    _access.expires_in = doc["expires_in"]   | 3600;
    _access.fetched_at = millis();

    if (_access.token.length() == 0) {
        log_e("No access_token in response");
        return false;
    }

    log_i("Access token refreshed, expires in %ds", _access.expires_in);
    return true;
}

int OAuth2Manager::parseExpiresIn(const String& raw) {
    // Most APIs return raw seconds as int
    return raw.toInt();
}

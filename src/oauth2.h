#ifndef OAUTH2_H
#define OAUTH2_H

#include <Arduino.h>

#define TOKEN_URL "https://oauth2.googleapis.com/token"

struct AccessToken {
    String token;
    int    expires_in;   // seconds from now
    uint64_t fetched_at; // millis() when fetched
};

class OAuth2Manager {
public:
    void setConfig(const String& clientId, const String& clientSecret, const String& refreshToken);

    // Get a valid access token — refreshes if expired
    bool getAccessToken(String& outToken);

    // Force refresh
    bool refreshAccessToken();

    bool isConfigured() { return _refresh_token.length() > 0; }

private:
    String _client_id;
    String _client_secret;
    String _refresh_token;
    AccessToken _access;

    // Try parsing an ISO 8601 duration like "3599s"
    int parseExpiresIn(const String& raw);
};

extern OAuth2Manager oauth2;

#endif

/*
 * Neon Calendar — Cardputer Firmware
 * Google Calendar client for M5Stack Cardputer
 *
 * Features:
 * - OAuth2 with refresh token (no backend dependency)
 * - WiFi manager with SD-based network config
 * - Google Calendar CRUD (view, add, edit, delete events)
 * - Offline cache with bidirectional sync
 * - Clean UI with keyboard navigation
 */

#include <Arduino.h>
#include <M5Cardputer.h>
#include <time.h>

#include "storage.h"
#include "wifi_manager.h"
#include "oauth2.h"
#include "calendar_api.h"
#include "sync.h"
#include "ui.h"

// ─── State ───
enum AppState {
    STATE_BOOT,
    STATE_WIFI_CONNECTING,
    STATE_AUTH_CHECK,
    STATE_SYNC,
    STATE_AGENDA,
    STATE_WIFI_SCAN,
    STATE_WIFI_PASSWORD,
    STATE_EVENT_DETAIL,
    STATE_EVENT_EDIT,
    STATE_EVENT_CREATE,
    STATE_SETTINGS,
    STATE_ERROR,
};

AppState state = STATE_BOOT;

// ─── Forward declarations ───
void bootSequence();
void handleWiFiSelect();
void handleAgendaInput();
void handleEventDetailInput();
void handleEventEditInput();
void handleWiFiScanInput();
void handleWiFiPasswordInput();
std::vector<CalendarEvent> loadTodayEvents();

// ─── Setup ───
void setup() {
    Serial.begin(115200);
    delay(100);

    // Init Cardputer hardware
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(COLOR_BG);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(20, 60);
    M5Cardputer.Display.println("Neon Calendar");
    M5Cardputer.Display.setCursor(20, 80);
    M5Cardputer.Display.println("Iniciando...");
    M5Cardputer.Display.setCursor(20, 100);
    M5Cardputer.Display.println("Carregando SD...");

    // Init storage
    if (!storage.begin()) {
        M5Cardputer.Display.setCursor(20, 120);
        M5Cardputer.Display.setTextColor(COLOR_RED);
        M5Cardputer.Display.println("ERRO: SD nao encontrado!");
        ui.showMessage("Insira o cartao SD", COLOR_RED, 10000);
    } else {
        M5Cardputer.Display.setCursor(20, 120);
        M5Cardputer.Display.setTextColor(COLOR_GREEN);
        M5Cardputer.Display.println("SD OK!");
    }

    // Set time from compile time (will be updated via NTP later)
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");

    ui.begin();
    bootSequence();
}

// ─── Main Loop ───
void loop() {
    M5Cardputer.update();
    ui.loop();

    switch (state) {
        case STATE_WIFI_SCAN:    handleWiFiSelect();    break;
        case STATE_WIFI_PASSWORD: handleWiFiPasswordInput(); break;
        case STATE_AGENDA:       handleAgendaInput();    break;
        case STATE_EVENT_DETAIL: handleEventDetailInput(); break;
        case STATE_EVENT_EDIT:
        case STATE_EVENT_CREATE: handleEventEditInput(); break;
        default: break;
    }

    delay(20);
}

// ─── Boot Sequence ───
void bootSequence() {
    // 1. Load auth
    M5Cardputer.Display.setCursor(20, 140);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.println("Carregando auth...");

    OAuthConfig auth;
    if (!storage.loadAuth(auth)) {
        M5Cardputer.Display.setCursor(20, 160);
        M5Cardputer.Display.setTextColor(COLOR_RED);
        M5Cardputer.Display.println("ERRO: auth.json nao encontrado!");
        ui.showMessage("Coloque auth.json no cartao SD", COLOR_RED, 10000);
        state = STATE_ERROR;
        return;
    }
    oauth2.setConfig(auth.client_id, auth.client_secret, auth.refresh_token);
    M5Cardputer.Display.setCursor(20, 160);
    M5Cardputer.Display.setTextColor(COLOR_GREEN);
    M5Cardputer.Display.println("Auth OK!");

    // 2. Wait for NTP
    M5Cardputer.Display.setCursor(20, 180);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.println("Sincronizando hora...");
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 1700000000 && attempts < 20) {
        delay(500);
        now = time(nullptr);
        attempts++;
    }
    if (now > 1700000000) {
        M5Cardputer.Display.setCursor(20, 200);
        M5Cardputer.Display.setTextColor(COLOR_GREEN);
        M5Cardputer.Display.println("Hora OK!");
    } else {
        M5Cardputer.Display.setCursor(20, 200);
        M5Cardputer.Display.setTextColor(COLOR_YELLOW);
        M5Cardputer.Display.println("Hora: usando fallback");
    }

    // 3. Try WiFi
    state = STATE_WIFI_CONNECTING;
    M5Cardputer.Display.setCursor(20, 220);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.println("Conectando WiFi...");

    if (wifiMgr.connectFromSaved()) {
        M5Cardputer.Display.setCursor(20, 220);
        M5Cardputer.Display.setTextColor(COLOR_GREEN);
        M5Cardputer.Display.printf("WiFi: %s", wifiMgr.currentSSID().c_str());

        // Try sync
        delay(500);
        if (syncMgr.fullSync()) {
            ui.setSyncIcon(true, syncMgr.pendingCount());
        } else {
            ui.showMessage("Sync parcial", COLOR_YELLOW, 5000);
            // Load local cache anyway
        }
    } else {
        M5Cardputer.Display.setCursor(20, 220);
        M5Cardputer.Display.setTextColor(COLOR_YELLOW);
        M5Cardputer.Display.println("WiFi: sem rede salva");
        ui.showMessage("Conecte-se ao WiFi", COLOR_YELLOW, 5000);
    }

    // Load events from cache (or empty)
    delay(1000);
    state = STATE_AGENDA;
    ui.setScreen(SCREEN_MAIN_AGENDA);
}

// ─── Load today's events ───
std::vector<CalendarEvent> loadTodayEvents() {
    std::vector<CalendarEvent> all;
    storage.loadEvents(all);

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char today[12];
    strftime(today, sizeof(today), "%Y-%m-%d", t);

    char tomorrow[12];
    t->tm_mday += 1;
    mktime(t);
    strftime(tomorrow, sizeof(tomorrow), "%Y-%m-%d", t);

    std::vector<CalendarEvent> todayEvents;
    for (auto& ev : all) {
        if (ev.deleted) continue;
        if (ev.start_date.substring(0, 10) <= String(today) &&
            ev.end_date.substring(0, 10) >= String(today)) {
            todayEvents.push_back(ev);
        }
    }

    return todayEvents;
}

// ─── WiFi Scan Input ───
void handleWiFiSelect() {
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            // ESC → back to agenda
            // ENTER → select network and connect
            // Arrow keys to navigate (mapped to ,/. for up/down)
        }
    }
}

// ─── WiFi Password Input ───
void handleWiFiPasswordInput() {
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            auto key = M5Cardputer.Keyboard.keys();
            // Handle keyboard input
        }
    }
}

// ─── Agenda Input ───
void handleAgendaInput() {
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            // Get key from keyboard buffer
            // We'll handle specific keys via the M5Cardputer API
        }
    }
}

// ─── Event Detail Input ───
void handleEventDetailInput() {}

// ─── Event Edit Input ───
void handleEventEditInput() {}

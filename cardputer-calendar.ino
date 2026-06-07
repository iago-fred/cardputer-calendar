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
#include <SD.h>       // must be before M5GFX for SPI sharing
#include <SPI.h>
#include <M5Cardputer.h>
#include <time.h>

#include "storage.h"
#include "wifi_manager.h"
#include "oauth2.h"
#include "calendar_api.h"
#include "sync.h"
#include "ui.h"
#include <algorithm>

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
    STATE_SYNCING,
    STATE_ERROR,
};

AppState state = STATE_BOOT;

// ─── Global references ───
std::vector<CalendarEvent> g_events;       // today's events (displayed)
std::vector<CalendarEvent> g_allEvents;    // full cache
int g_selectedEvent = 0;
String g_wifiPassword;
String g_selectedSSID;

// ─── Forward declarations ───
void bootSequence();
void loadAndShowAgenda();
void handleAgendaInput();
void handleEventDetailInput();
void handleEventEditInput(String& buffer, bool& done, bool& save);
void handleWiFiSelect();
void handleWiFiPasswordInput();

// ─── Keyboard helper ───
typedef struct { uint8_t key; bool shift; } key_event_t;

// M5Cardputer keyboard matrix: keyList() returns Point2D_t (x=col, y=row)
// Matrix mapeada manualmente por Iago via keyboard_mapper sketch
// 4 linhas (y=0..3) x 14 colunas (x=0..13)
// 127=backspace, 9=tab, 10=enter\n, 0=modificador (pular)
static const uint8_t KEY_MATRIX[4][14] = {
    //     0   1   2   3   4   5   6   7   8   9  10  11  12  13
    /* 0 */'`','1','2','3','4','5','6','7','8','9','0','-','=',127,
    /* 1 */  9,'q','w','e','r','t','y','u','i','o','p','[',']','\\',
    /* 2 */  0,  0,'a','s','d','f','g','h','j','k','l',';','\'','\n',
    /* 3 */  0,  0,  0,'z','x','c','v','b','n','m',',','.','/',' ',
};

// Shifted symbols for keys that change with shift
static uint8_t shiftSymbol(uint8_t key) {
    switch (key) {
        case '`': return '~';
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default:  return key;
    }
}

key_event_t readKey() {
    if (!M5Cardputer.Keyboard.isChange()) return {0, false};
    if (!M5Cardputer.Keyboard.isPressed()) return {0, false};

    auto keys = M5Cardputer.Keyboard.keyList();
    if (keys.empty()) return {0, false};

    auto status = M5Cardputer.Keyboard.keysState();

    // Check for fn+backtick = ESC (fn=[2][0], backtick=[0][0])
    bool fnPressed = false, backtickPressed = false;
    for (auto& k : keys) {
        if (k.x == 0 && k.y == 2) fnPressed = true;
        if (k.x == 0 && k.y == 0) backtickPressed = true;
    }
    if (fnPressed && backtickPressed) {
        return {0x1B, status.shift};  // ESC
    }

    // Iterate through ALL pressed keys, skip modifiers (value 0 in matrix)
    uint8_t key = 0;
    for (auto& k : keys) {
        if (k.y < 0 || k.y >= 4 || k.x < 0 || k.x >= 14) continue;
        uint8_t c = KEY_MATRIX[k.y][k.x];
        if (c != 0 && !(k.x == 0 && k.y == 0)) { key = c; break; }  // skip backtick key
    }
    // If only backtick pressed (no fn), use it
    if (key == 0 && backtickPressed && !fnPressed) {
        key = '`';
    }
    if (key == 0) return {0, false};

    // Apply shift: letters uppercase, symbols shifted
    if (status.shift) {
        if (key >= 'a' && key <= 'z') {
            key = toupper(key);
        } else {
            key = shiftSymbol(key);
        }
    }

    return {key, status.shift};
}

// ─── Setup ───
void setup() {
    Serial.begin(115200);
    delay(100);

    // Init Cardputer hardware
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(20, 60);
    M5Cardputer.Display.println("Neon Calendar");
    M5Cardputer.Display.setCursor(20, 80);
    M5Cardputer.Display.println("Iniciando...");
    M5Cardputer.Display.setCursor(20, 100);
    M5Cardputer.Display.println("SD card...");

    // Init storage
    if (!storage.begin()) {
        M5Cardputer.Display.setCursor(20, 120);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.println("ERRO: SD nao encontrado!");
    } else {
        M5Cardputer.Display.setCursor(20, 120);
        M5Cardputer.Display.setTextColor(TFT_GREEN);
        M5Cardputer.Display.println("SD OK!");
    }

    // Set timezone (BRT = UTC-3)
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");

    ui.begin();
    bootSequence();
}

// ─── Main Loop ───
void loop() {
    M5Cardputer.update();
    ui.loop();

    switch (state) {
        case STATE_AGENDA:       handleAgendaInput();        break;
        case STATE_EVENT_DETAIL: handleEventDetailInput();    break;
        case STATE_EVENT_EDIT:
        case STATE_EVENT_CREATE: {
            static String editBuffer;
            static bool firstEnter = true;
            if (firstEnter) {
                editBuffer = "";
                firstEnter = false;
            }
            static bool editDone = false;
            static bool editSave = false;
            handleEventEditInput(editBuffer, editDone, editSave);
            if (editDone) {
                if (editSave && editBuffer.length() > 0) {
                    CalendarEvent newEvt;
                    newEvt.summary = editBuffer;
                    newEvt.start_date = CalendarAPI::todayMin();
                    newEvt.end_date = CalendarAPI::todayMax();
                    newEvt.is_all_day = true;
                    newEvt.dirty = true;
                    newEvt.id = "";

                    if (state == STATE_EVENT_CREATE) {
                        PendingChange pc;
                        pc.action = "create";
                        pc.event = newEvt;
                        storage.addPendingChange(pc);
                        g_allEvents.push_back(newEvt);
                        storage.saveEvents(g_allEvents);
                        ui.showMessage("Criado (pendente sync)", TFT_GREEN, 2000);
                    }
                }
                editDone = false;
                editSave = false;
                firstEnter = true;
                state = STATE_AGENDA;
                loadAndShowAgenda();
            }
            break;
        }
        case STATE_WIFI_SCAN:    handleWiFiSelect();         break;
        case STATE_WIFI_PASSWORD: handleWiFiPasswordInput();  break;
        case STATE_SYNCING: {
            if (!storage.isReady()) {
                ui.showMessage("ERRO: SD nao encontrado!", TFT_RED, 3000);
                state = STATE_AGENDA;
                break;
            }
            ui.showMessage("Sincronizando...", TFT_YELLOW, 0);
            if (syncMgr.fullSync()) {
                ui.setSyncIcon(true, syncMgr.pendingCount());
                ui.showMessage("Sync OK!", TFT_GREEN, 2000);
            } else {
                ui.showMessage("Sync falhou", TFT_RED, 3000);
            }
            loadAndShowAgenda();
            state = STATE_AGENDA;
            break;
        }
        default: break;
    }

    delay(20);
}

// ─── Boot Sequence ───
void bootSequence() {
    if (!storage.isReady()) {
        M5Cardputer.Display.setCursor(20, 140);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.println("Sem SD - Modo limitado");
        delay(2000);
        state = STATE_AGENDA;
        loadAndShowAgenda();
        return;
    }

    // 1. Load auth
    M5Cardputer.Display.setCursor(20, 140);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.println("Auth...");

    OAuthConfig auth;
    if (!storage.loadAuth(auth)) {
        M5Cardputer.Display.setCursor(20, 160);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.println("auth.json nao encontrado!");
        delay(2000);
        state = STATE_AGENDA;
        loadAndShowAgenda();
        return;
    }
    oauth2.setConfig(auth.client_id, auth.client_secret, auth.refresh_token);
    M5Cardputer.Display.setCursor(20, 160);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.println("Auth OK!");

    // 2. Wait for NTP (max 10s)
    M5Cardputer.Display.setCursor(20, 180);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.println("NTP...");
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 1700000000 && attempts < 50) {
        delay(200);
        now = time(nullptr);
        attempts++;
    }

    // 3. Try WiFi
    state = STATE_WIFI_CONNECTING;
    M5Cardputer.Display.setCursor(20, 200);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.println("WiFi...");

    if (wifiMgr.connectFromSaved()) {
        M5Cardputer.Display.printf("WiFi: %s\n", wifiMgr.currentSSID().c_str());

        // Try sync
        delay(300);
        if (syncMgr.fullSync()) {
            ui.setSyncIcon(true, syncMgr.pendingCount());
            M5Cardputer.Display.println("Sync OK!");
        } else {
            M5Cardputer.Display.println("Sync: offline");
        }
    } else {
        M5Cardputer.Display.setCursor(20, 220);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.println("Offline");
    }

    delay(1000);
    state = STATE_AGENDA;
    loadAndShowAgenda();
}

// ─── Load today's events and show agenda ───
void loadAndShowAgenda() {
    g_events.clear();
    g_allEvents.clear();
    storage.loadEvents(g_allEvents);

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char today[12];
    strftime(today, sizeof(today), "%Y-%m-%d", t);

    char tomorrow[12];
    t->tm_mday += 1;
    mktime(t);
    strftime(tomorrow, sizeof(tomorrow), "%Y-%m-%d", t);

    for (auto& ev : g_allEvents) {
        if (ev.deleted) continue;
        // Check if event overlaps with today
        String evStart = ev.start_date.substring(0, 10);
        String evEnd   = ev.end_date.substring(0, 10);
        if (evStart <= String(tomorrow) && evEnd >= String(today)) {
            g_events.push_back(ev);
        }
    }

    g_selectedEvent = 0;
    ui.showAgenda(g_events);
}

// ─── Agenda Input ───
void handleAgendaInput() {
    auto k = readKey();
    if (k.key == 0) return;

    if (k.key == 'w') {
        // Start WiFi scan
        state = STATE_WIFI_SCAN;
        ui.showWifiScan();
        return;
    }

    if (k.key == 's') {
        // Trigger sync
        if (!storage.isReady()) {
            ui.showMessage("ERRO: SD nao encontrado!", TFT_RED, 3000);
            return;
        }
        state = STATE_SYNCING;
        return;
    }

    if (k.key == '+') {
        // Create new event
        state = STATE_EVENT_CREATE;
        ui.showEventEditor(CalendarEvent());
        return;
    }

    // Navigation: , = up, / = down, SET = select, ESC = nothing
    if (k.key == ',') { // Up (M5Cardputer: , key = up)
        if (g_selectedEvent > 0) g_selectedEvent--;
        ui.showAgenda(g_events);
    }
    else if (k.key == '.') { // Down (M5Cardputer: . key = down)
        if (g_selectedEvent < (int)g_events.size() - 1) g_selectedEvent++;
        ui.showAgenda(g_events);
    }
    else if (k.key == '\n' || k.key == '\r') { // SET/Enter
        if (g_events.size() > 0 && g_selectedEvent < (int)g_events.size()) {
            state = STATE_EVENT_DETAIL;
            ui.showEventDetail(g_events[g_selectedEvent]);
        }
    }
}

// ─── Event Detail Input ───
void handleEventDetailInput() {
    auto k = readKey();
    if (k.key == 0) return;

    if (k.key == 0x1B) { // ESC
        state = STATE_AGENDA;
        loadAndShowAgenda();
        return;
    }

    if (k.key == 'd' || k.key == 0x7F) { // Delete
        if (g_selectedEvent < (int)g_events.size()) {
            auto& ev = g_events[g_selectedEvent];
            g_allEvents.erase(
                std::remove_if(g_allEvents.begin(), g_allEvents.end(),
                    [&](const CalendarEvent& e) { return e.id == ev.id; }),
                g_allEvents.end());
            storage.saveEvents(g_allEvents);
            PendingChange pc;
            pc.action = "delete";
            pc.event = ev;
            storage.addPendingChange(pc);
            ui.showMessage("Excluido (pendente sync)", TFT_RED, 2000);
            state = STATE_AGENDA;
            loadAndShowAgenda();
        }
    }

    if (k.key == 'e') { // Edit
        ui.showMessage("Edicao via Google", TFT_YELLOW, 2000);
    }
}

// ─── Event Edit Input ───
void handleEventEditInput(String& buffer, bool& done, bool& save) {
    auto k = readKey();
    if (k.key == 0) return;

    if (k.key == 0x1B) { // ESC
        done = true;
        save = false;
        return;
    }

    if (k.key == '\n' || k.key == '\r') { // Enter = save
        done = true;
        save = true;
        return;
    }

    if (k.key == 0x08 || k.key == 0x7F) { // Backspace
        if (buffer.length() > 0) buffer.remove(buffer.length() - 1);
    } else if (k.key >= 0x20 && k.key <= 0x7E) {
        // Printable ASCII — readKey() already applies shift/caps
        buffer += (char)k.key;
    }

    // Update display
    CalendarEvent dummy;
    dummy.start_date = CalendarAPI::todayMin();
    ui.showEventEditor(dummy);
}

// ─── WiFi Scan Input ───
static std::vector<ScannedNetwork> g_scanResults; // persist results across calls

void handleWiFiSelect() {
    auto k = readKey();
    if (k.key == 0) return;

    if (k.key == 0x1B) { // ESC
        g_scanResults.clear();
        state = STATE_AGENDA;
        loadAndShowAgenda();
        return;
    }

    // Number selection (1-9): select network and go to password screen
    if (k.key >= '1' && k.key <= '9') {
        int idx = k.key - '1';
        if (idx < (int)g_scanResults.size()) {
            g_selectedSSID = g_scanResults[idx].ssid;
            g_wifiPassword = "";
            state = STATE_WIFI_PASSWORD;
            ui.showWifiPasswordPrompt(g_selectedSSID);
        }
        return;
    }

    // Scan on enter
    if (k.key == '\n' || k.key == '\r') {
        ui.showMessage("Escaneando...", TFT_YELLOW, 0);

        g_scanResults.clear();
        if (wifiMgr.scan(g_scanResults, true)) {
            // Show first N available
            M5Cardputer.Display.fillScreen(COLOR_BG);
            M5Cardputer.Display.setTextColor(TFT_WHITE, COLOR_BG);
            M5Cardputer.Display.setCursor(4, 20);
            M5Cardputer.Display.println("Redes encontradas:");
            int y = 40;
            for (size_t i = 0; i < g_scanResults.size() && i < 10; i++) {
                M5Cardputer.Display.setCursor(8, y);
                M5Cardputer.Display.printf("%d. %s (%ddBm)", i+1, g_scanResults[i].ssid.c_str(), g_scanResults[i].rssi);
                y += 14;
            }
            M5Cardputer.Display.setCursor(4, y + 10);
            M5Cardputer.Display.println("Digite o numero");
        } else {
            ui.showMessage("Nenhuma rede", TFT_RED, 2000);
        }
    }
}

// ─── WiFi Password Input ───
void handleWiFiPasswordInput() {
    auto k = readKey();
    if (k.key == 0) return;

    if (k.key == 0x1B) { // ESC
        g_wifiPassword = "";
        g_selectedSSID = "";
        state = STATE_WIFI_SCAN;
        ui.showWifiScan();
        return;
    }

    if (k.key == '\n' || k.key == '\r') {
        if (g_selectedSSID.length() == 0) {
            ui.showMessage("Selecione rede primeiro", TFT_RED, 2000);
            state = STATE_WIFI_SCAN;
            return;
        }
        if (wifiMgr.connect(g_selectedSSID, g_wifiPassword, 15000)) {
            storage.addWifiNetwork({g_selectedSSID, g_wifiPassword});
            ui.showMessage("Conectado!", TFT_GREEN, 2000);
            g_wifiPassword = "";
            g_selectedSSID = "";
            state = STATE_AGENDA;
            loadAndShowAgenda();
        } else {
            ui.showMessage("Falhou", TFT_RED, 3000);
        }
        return;
    }

    if (k.key == 0x08 || k.key == 0x7F) {
        if (g_wifiPassword.length() > 0) g_wifiPassword.remove(g_wifiPassword.length() - 1);
    } else if (k.key >= 0x20 && k.key <= 0x7E) {
        g_wifiPassword += (char)k.key;
    }

    ui.setInputBuffer(g_wifiPassword);
    ui.showWifiPasswordPrompt(g_selectedSSID, g_wifiPassword);
}

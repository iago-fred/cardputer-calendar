#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include "storage.h"
#include <vector>

// ─── Screen IDs ───
enum Screen {
    SCREEN_BOOT,
    SCREEN_WIFI_SELECT,
    SCREEN_WIFI_ENTER_PASS,
    SCREEN_MAIN_AGENDA,
    SCREEN_EVENT_DETAIL,
    SCREEN_EVENT_EDIT,
    SCREEN_EVENT_CREATE,
    SCREEN_SETTINGS,
    SCREEN_MESSAGE,    // temporary message/error overlay
};

// ─── Color Theme ───
// 320x240 ST7789 — M5Cardputer
// Colors match Google Calendar: https://learn.microsoft.com/en-us/graph/api/resources/calendar

#define COLOR_BG       0x39E7  // Dark gray
#define COLOR_SURFACE  0x5AEB  // Medium gray
#define COLOR_TEXT     0xFFFF  // White
#define COLOR_MUTED    0xB596  // Light gray
#define COLOR_ACCENT   0x7E4F  // Neon blue
#define COLOR_GREEN    0x3603  // Sage — personal
#define COLOR_RED      0xF800  // Tomato — done
#define COLOR_BLUE     0x6D9B  // Blueberry — professional
#define COLOR_YELLOW   0xFF60  // Banana — college
#define COLOR_PURPLE   0x801F  // Grape — birthdays

struct UIManager {
    void begin();
    void loop();  // Handle input, refresh screen

    // Navigation
    void setScreen(Screen s);
    Screen currentScreen() { return _screen; }

    // Status message
    void showMessage(const String& msg, uint16_t color = COLOR_TEXT, uint32_t durationMs = 3000);

    // ─── Screen-specific data ───

    // WiFi
    void showWifiScan();
    void showWifiPasswordPrompt(const String& ssid);
    void wifiScanResult(const String& ssid);

    // Events
    void showAgenda(const std::vector<CalendarEvent>& events);
    void showEventDetail(const CalendarEvent& evt);
    void showEventEditor(const CalendarEvent& evt);  // create or edit

    // ─── Sync status ───
    void setSyncIcon(bool synced, int pendingCount);

    // ─── Caps access for input handling ───
    bool isCaps() { return _caps; }

private:
    Screen _screen = SCREEN_BOOT;
    String _message;
    uint16_t _msgColor = COLOR_TEXT;
    uint32_t _msgUntil = 0;
    bool _msgActive = false;

    bool _synced = false;
    int  _pendingCount = 0;

    // Input buffer (for text entry in edit mode)
    String _inputBuffer;
    int    _cursorPos;
    bool   _shift = false;
    bool   _caps = false;
    int    _selectedIndex = 0;
    int    _scrollOffset = 0;

    void drawTopBar();
    void drawBottomBar(const String& leftHint, const String& rightHint);
    void drawMessage();

    // Drawing helpers
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawString(int x, int y, const String& text, uint16_t color, int fontSize = 1);
    void drawChar(int x, int y, char c, uint16_t color, int fontSize = 1);
    int  textWidth(const String& text, int fontSize = 1);

    uint16_t colorForId(const String& colorId);
};

extern UIManager ui;

#endif

#include "ui.h"
#include <M5Cardputer.h>
#include <M5GFX.h>

UIManager ui;

// ─── Init ───
void UIManager::begin() {
    M5Cardputer.Display.setRotation(1); // landscape
    M5Cardputer.Display.fillScreen(COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
}

void UIManager::loop() {
    M5Cardputer.update();
    // Message timer
    if (_msgActive && millis() > _msgUntil) {
        _msgActive = false;
    }
}

// ─── Screen switching ───
void UIManager::setScreen(Screen s) {
    _screen = s;
    _selectedIndex = 0;
    _scrollOffset = 0;
    _inputBuffer = "";
    _cursorPos = 0;
    _shift = false;
    _caps = false;
    M5Cardputer.Display.fillScreen(COLOR_BG);
}

void UIManager::showMessage(const String& msg, uint16_t color, uint32_t durationMs) {
    _message = msg;
    _msgColor = color;
    _msgActive = true;
    _msgUntil = millis() + durationMs;
    // Draw immediately
    drawMessage();
}

// ─── Top bar ───
void UIManager::drawTopBar() {
    M5Cardputer.Display.fillRect(0, 0, 320, 16, COLOR_SURFACE);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    M5Cardputer.Display.setCursor(4, 3);
    M5Cardputer.Display.print("Neon Calendar");

    // Sync status
    String status;
    if (_pendingCount > 0) {
        status = " P:" + String(_pendingCount);
        M5Cardputer.Display.setTextColor(COLOR_YELLOW, COLOR_SURFACE);
    } else if (_synced) {
        status = " OK";
        M5Cardputer.Display.setTextColor(COLOR_GREEN, COLOR_SURFACE);
    } else {
        status = " --";
        M5Cardputer.Display.setTextColor(COLOR_MUTED, COLOR_SURFACE);
    }
    M5Cardputer.Display.setCursor(310 - textWidth(status), 3);
    M5Cardputer.Display.print(status);
}

// ─── Bottom bar ───
void UIManager::drawBottomBar(const String& leftHint, const String& rightHint) {
    M5Cardputer.Display.fillRect(0, 228, 320, 12, COLOR_SURFACE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_MUTED, COLOR_SURFACE);
    M5Cardputer.Display.setCursor(4, 229);
    M5Cardputer.Display.print(leftHint);
    M5Cardputer.Display.setTextColor(COLOR_MUTED, COLOR_SURFACE);
    M5Cardputer.Display.setCursor(320 - 4 - textWidth(rightHint), 229);
    M5Cardputer.Display.print(rightHint);
}

// ─── Message overlay ───
void UIManager::drawMessage() {
    if (!_msgActive) return;
    M5Cardputer.Display.fillRect(20, 100, 280, 40, COLOR_SURFACE);
    M5Cardputer.Display.drawRect(20, 100, 280, 40, COLOR_ACCENT);
    M5Cardputer.Display.setTextColor(_msgColor, COLOR_SURFACE);
    M5Cardputer.Display.setTextSize(1);
    int16_t x = 160 - (textWidth(_message) / 2);
    M5Cardputer.Display.setCursor(max(x, 24), 112);
    M5Cardputer.Display.print(_message);
}

// ─── Color mapping ───
uint16_t UIManager::colorForId(const String& colorId) {
    if (colorId == "2")  return COLOR_GREEN;
    if (colorId == "11") return COLOR_RED;
    if (colorId == "9")  return COLOR_BLUE;
    if (colorId == "5")  return COLOR_YELLOW;
    if (colorId == "3")  return COLOR_PURPLE;
    if (colorId == "1")  return COLOR_ACCENT;
    if (colorId == "4")  return COLOR_ACCENT;
    if (colorId == "6")  return 0xFD20; // orange
    if (colorId == "7")  return 0xCA8B; // teal
    if (colorId == "8")  return 0xFFFF; // white
    if (colorId == "10") return 0xDEFB; // light blue
    return COLOR_TEXT;
}

// ─── Drawing helpers ───
void UIManager::fillRect(int x, int y, int w, int h, uint16_t color) {
    M5Cardputer.Display.fillRect(x, y, w, h, color);
}

void UIManager::drawString(int x, int y, const String& text, uint16_t color, int fontSize) {
    M5Cardputer.Display.setTextSize(fontSize);
    M5Cardputer.Display.setTextColor(color, COLOR_BG);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(text);
}

void UIManager::drawChar(int x, int y, char c, uint16_t color, int fontSize) {
    M5Cardputer.Display.setTextSize(fontSize);
    M5Cardputer.Display.setTextColor(color, COLOR_BG);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(c);
}

int UIManager::textWidth(const String& text, int fontSize) {
    M5Cardputer.Display.setTextSize(fontSize);
    return M5Cardputer.Display.textWidth(text);
}

// ─── WiFi Scan Screen ───
void UIManager::showWifiScan() {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawString(4, 20, "WiFi - Scan", COLOR_ACCENT, 2);
    drawString(4, 42, "[Enter] para escanear", COLOR_MUTED);
    drawBottomBar("ESC: voltar", "ENT: scan");
}

void UIManager::showWifiPasswordPrompt(const String& ssid, const String& currentPassword) {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawString(4, 20, "Senha: " + ssid, COLOR_TEXT);
    String masked;
    const String& pw = currentPassword.length() > 0 ? currentPassword : _inputBuffer;
    for (size_t i = 0; i < pw.length(); i++) masked += '*';
    drawString(4, 50, "> " + masked + (millis() % 1000 < 500 ? "|" : " "), COLOR_ACCENT);
    drawBottomBar("ESC: cancelar", "ENT: conectar");
}

// ─── Agenda Screen ───
void UIManager::showAgenda(const std::vector<CalendarEvent>& events) {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawTopBar();

    // Date header
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%a, %d/%m", t);
    drawString(160 - textWidth(String(buf), 2) / 2, 20, String(buf), COLOR_ACCENT, 2);

    if (events.empty()) {
        drawString(10, 60, "Nenhum evento hoje", COLOR_MUTED);
        drawString(10, 80, "[+] Novo  [W] WiFi  [S] Sync", COLOR_MUTED);
        drawBottomBar("SET: menu", "W:WiFi S:Sync");
        drawMessage();
        return;
    }

    int y = 42;
    for (size_t i = 0; i < events.size(); i++) {
        if ((int)i < _scrollOffset) continue;
        if (y > 220) break;

        auto& ev = events[i];
        uint16_t c = colorForId(ev.color_id);

        // Color bar
        fillRect(2, y, 4, 16, c);

        // Time
        String time;
        if (ev.is_all_day) time = "ALL";
        else time = ev.start_date.substring(11, 16);

        drawString(10, y, time, COLOR_MUTED);

        // Summary (truncate to fit)
        String summary = ev.summary;
        if (textWidth(summary, 1) > 250) {
            while (textWidth(summary + "...", 1) > 250) summary.remove(summary.length() - 1);
            summary += "...";
        }
        drawString(50, y, summary, c);

        // Selection indicator
        if ((int)i == _selectedIndex) {
            fillRect(0, y - 1, 320, 18, 0x0841); // subtle highlight
            drawString(50, y, summary, COLOR_ACCENT);
            drawString(0, y, ">", COLOR_ACCENT);
        }

        y += 18;
    }

    drawBottomBar(",. :navegar", "ENT:abrir");
    drawMessage();
}

// ─── Event Detail Screen ───
void UIManager::showEventDetail(const CalendarEvent& evt) {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawTopBar();

    uint16_t c = colorForId(evt.color_id);
    drawString(4, 22, evt.summary, c, 2);

    // Date/time
    String timeStr;
    if (evt.is_all_day) {
        timeStr = "Data: " + evt.start_date.substring(0, 10);
        if (evt.start_date != evt.end_date)
            timeStr += " -> " + evt.end_date.substring(0, 10);
    } else {
        timeStr = "Horario: " + evt.start_date.substring(0, 16) + " - " + evt.end_date.substring(11, 16);
    }
    drawString(4, 50, timeStr, COLOR_TEXT);

    // Description
    if (evt.description.length() > 0) {
        drawString(4, 75, evt.description, COLOR_MUTED);
    }

    // Color indicator
    drawString(4, 200, "Cor: " + evt.color_id, c);

    drawBottomBar("ESC: voltar", "D:excluir E:editar");
    drawMessage();
}

// ─── Event Editor Screen ───
void UIManager::showEventEditor(const CalendarEvent& evt) {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawTopBar();

    String title = evt.id.length() > 0 ? "Editar" : "Novo Evento";
    drawString(4, 22, title, COLOR_ACCENT, 2);

    // Show current field
    int y = 50;
    drawString(4, y, "Titulo:", COLOR_MUTED);
    String displayText = _inputBuffer.length() > 0 ? _inputBuffer : "(digite e ENTER)";
    drawString(4, y + 16, "> " + displayText + (millis() % 1000 < 500 ? "|" : " "), COLOR_ACCENT);

    drawBottomBar("ESC: cancelar", "ENT: salvar");
    drawMessage();
}

// ─── Sync status ───
void UIManager::setSyncIcon(bool synced, int pendingCount) {
    _synced = synced;
    _pendingCount = pendingCount;
}

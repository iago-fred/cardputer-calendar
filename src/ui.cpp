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
    M5Cardputer.Display.fillScreen(COLOR_BG);
}

void UIManager::showMessage(const String& msg, uint16_t color, uint32_t durationMs) {
    _message = msg;
    _msgColor = color;
    _msgActive = true;
    _msgUntil = millis() + durationMs;
}

// ─── Top bar ───
void UIManager::drawTopBar() {
    M5Cardputer.Display.fillRect(0, 0, 320, 16, COLOR_SURFACE);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    M5Cardputer.Display.setCursor(4, 3);
    M5Cardputer.Display.print("Neon Calendar");

    // Sync status
    if (_synced) {
        M5Cardputer.Display.setTextColor(COLOR_GREEN, COLOR_SURFACE);
        M5Cardputer.Display.setCursor(310 - 30, 3);
        M5Cardputer.Display.print("✓");
    }

    // Pending count
    if (_pendingCount > 0) {
        M5Cardputer.Display.setTextColor(COLOR_YELLOW, COLOR_SURFACE);
        M5Cardputer.Display.setCursor(310 - 50, 3);
        M5Cardputer.Display.printf("P:%d", _pendingCount);
    }
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
    // Semi-transparent box
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
    drawString(4, 20, "📶 Redes WiFi", COLOR_TEXT, 2);
    drawString(4, 42, "Escaneando...", COLOR_MUTED);
    drawBottomBar("ESC: voltar", "SET: confirmar");
}

void UIManager::showWifiPasswordPrompt(const String& ssid) {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawString(4, 20, "Senha: " + ssid, COLOR_TEXT);
    drawString(4, 50, "> " + _inputBuffer + (millis() % 1000 < 500 ? "|" : " "), COLOR_ACCENT);
    drawBottomBar("ESC: cancelar", "ENTER: conectar");
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
        drawBottomBar("SET: menu", "WiFi: sync");
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
        if (ev.is_all_day) time = "🌅";
        else time = ev.start_date.substring(11, 16);

        drawString(10, y, time, COLOR_MUTED);
        drawString(50, y, ev.summary, (i == (size_t)_selectedIndex) ? COLOR_ACCENT : c);

        y += 18;
    }

    drawBottomBar("▲▼: navegar", "SET: abrir");
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
        timeStr = "📅 " + evt.start_date.substring(0, 10);
        if (evt.start_date != evt.end_date)
            timeStr += " → " + evt.end_date.substring(0, 10);
    } else {
        timeStr = "🕐 " + evt.start_date.substring(0, 16) + " → " + evt.end_date.substring(11, 16);
    }
    drawString(4, 50, timeStr, COLOR_TEXT);

    // Description
    if (evt.description.length() > 0) {
        int y = 75;
        drawString(4, y, evt.description, COLOR_MUTED);
    }

    // Color indicator
    drawString(4, 200, "Cor: " + evt.color_id, c);

    drawBottomBar("ESC: voltar", "DEL: excluir");
    drawMessage();
}

// ─── Event Editor Screen ───
void UIManager::showEventEditor(const CalendarEvent& evt) {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawTopBar();

    drawString(4, 22, evt.id.length() > 0 ? "✏️ Editar" : "➕ Novo Evento", COLOR_ACCENT, 2);

    // Show current fields being edited
    int y = 50;
    drawString(4, y, "📝 Título: ", COLOR_MUTED);
    drawString(80, y, _inputBuffer.length() > 0 ? _inputBuffer : "(digite)", COLOR_TEXT);
    y += 20;
    drawString(4, y, "📅 Data: ", COLOR_MUTED);
    drawString(80, y, evt.start_date.length() > 0 ? evt.start_date.substring(0, 10) : "hoje", COLOR_TEXT);

    drawBottomBar("ESC: cancelar", "↵: salvar");
    drawMessage();
}

// ─── Settings Screen ───
void UIManager::setSyncIcon(bool synced, int pendingCount) {
    _synced = synced;
    _pendingCount = pendingCount;
}

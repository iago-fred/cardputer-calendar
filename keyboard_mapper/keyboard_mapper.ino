/**
 * Keyboard Mapper — M5Cardputer
 * Pressione cada tecla e veja as coordenadas (x, y) no display e Serial.
 * Anote os valores e me mande!
 */

#include <M5Cardputer.h>

void setup() {
    Serial.begin(115200);
    delay(100);

    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(10, 30);
    M5Cardputer.Display.println("Keyboard Mapper");
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(10, 60);
    M5Cardputer.Display.println("Aperte qualquer tecla");
    M5Cardputer.Display.setCursor(10, 75);
    M5Cardputer.Display.println("Serial: 115200 baud");

    Serial.println("\n\n=== KEYBOARD MAPPER ===");
    Serial.println("Pressione uma tecla de cada vez.");
    Serial.println("Formato: KEY: x=<col> y=<row>");
    Serial.println("Para teclas especiais (Enter, Tab, Bksp, Esc):");
    Serial.println("  aperte e anote as coordenadas.");
    Serial.println("========================\n");
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        auto keys = M5Cardputer.Keyboard.keyList();

        if (!keys.empty()) {
            int x = keys[0].x;
            int y = keys[0].y;

            Serial.printf("KEY: x=%d y=%d\n", x, y);

            // Show on display
            M5Cardputer.Display.fillScreen(TFT_BLACK);
            M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.setCursor(10, 30);
            M5Cardputer.Display.printf("x=%d  y=%d", x, y);
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setCursor(10, 60);
            M5Cardputer.Display.println("No Serial tambem!");

            // Aguarda soltar a tecla
            while (M5Cardputer.Keyboard.isPressed()) {
                M5Cardputer.update();
                delay(10);
            }

            M5Cardputer.Display.setCursor(10, 80);
            M5Cardputer.Display.println("OK! Proxima tecla...");
        }
    }

    delay(20);
}

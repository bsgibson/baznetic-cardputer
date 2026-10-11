#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_system.h>
#include "system_monitor.h"

// ====================================
// BAZNETIC SYSTEM MONITOR v2
// Integrated into Baznetic OS
// ====================================

namespace {

constexpr uint16_t BG_COLOR     = 0x0000;
constexpr uint16_t HEADER_COLOR = 0x07FF;
constexpr uint16_t TEXT_COLOR   = 0xFFFF;
constexpr uint16_t GREEN_COLOR  = 0x07E0;
constexpr uint16_t YELLOW_COLOR = 0xFFE0;
constexpr uint16_t RED_COLOR    = 0xF800;
constexpr uint16_t GREY_COLOR   = 0x8410;

unsigned long lastUpdate = 0;
bool exitRequested = false;

// ------------------------------------
// Draw header
// ------------------------------------

void drawHeader() {

    auto& d = M5Cardputer.Display;

    d.fillScreen(BG_COLOR);

    d.setTextSize(2);
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 5);
    d.println("BAZNETIC SYS");

    d.drawLine(0, 28, 240, 28, HEADER_COLOR);
}

// ------------------------------------
// Draw RAM usage bar
// ------------------------------------

void drawBar(int x, int y, int width, int percent) {

    percent = constrain(percent, 0, 100);

    uint16_t colour = GREEN_COLOR;

    if (percent >= 85) {
        colour = RED_COLOR;
    } else if (percent >= 65) {
        colour = YELLOW_COLOR;
    }

    auto& d = M5Cardputer.Display;

    d.drawRect(x, y, width, 9, GREY_COLOR);

    int fillWidth = (width - 2) * percent / 100;

    d.fillRect(x + 1, y + 1, fillWidth, 7, colour);
}

// ------------------------------------
// Temperature colour
// ------------------------------------

uint16_t getTempColor(float temp) {

    if (temp < 45.0) {
        return GREEN_COLOR;
    }

    if (temp < 60.0) {
        return YELLOW_COLOR;
    }

    return RED_COLOR;
}

// ------------------------------------
// Draw system information
// ------------------------------------

void drawSystemInfo() {

    drawHeader();

    auto& d = M5Cardputer.Display;

    // CPU frequency
    uint32_t cpuMHz = ESP.getCpuFreqMHz();

    // Internal ESP32-S3 temperature
    float temperature = temperatureRead();

    // Memory
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t usedHeap = totalHeap - freeHeap;

    int ramPercent = 0;

    if (totalHeap > 0) {
        ramPercent = (usedHeap * 100) / totalHeap;
    }

    // Flash
    uint32_t flashMB =
        ESP.getFlashChipSize() / (1024 * 1024);

    // Uptime
    unsigned long seconds = millis() / 1000;
    unsigned long hours = seconds / 3600;
    unsigned long minutes = (seconds % 3600) / 60;
    unsigned long secs = seconds % 60;

    // Battery
    int battery = M5Cardputer.Power.getBatteryLevel();

    d.setTextSize(1);

    // CPU
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 34);
    d.print("CPU");

    d.setTextColor(TEXT_COLOR);
    d.setCursor(70, 34);
    d.printf("%lu MHz", (unsigned long)cpuMHz);

    // Temperature
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 48);
    d.print("TEMP");

    d.setTextColor(getTempColor(temperature));
    d.setCursor(70, 48);
    d.printf("%.1f C", temperature);

    // RAM
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 62);
    d.print("RAM");

    d.setTextColor(TEXT_COLOR);
    d.setCursor(70, 62);

    d.printf(
        "%lu / %lu KB",
        (unsigned long)(usedHeap / 1024),
        (unsigned long)(totalHeap / 1024)
    );

    drawBar(70, 74, 150, ramPercent);

    // Flash
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 88);
    d.print("FLASH");

    d.setTextColor(TEXT_COLOR);
    d.setCursor(70, 88);
    d.printf("%lu MB", (unsigned long)flashMB);

    // Battery
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 101);
    d.print("BATTERY");

    d.setTextColor(TEXT_COLOR);
    d.setCursor(70, 101);

    if (battery >= 0 && battery <= 100) {
        d.printf("%d%%", battery);
    } else {
        d.print("N/A");
    }

    // Uptime
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 114);
    d.print("UPTIME");

    d.setTextColor(TEXT_COLOR);
    d.setCursor(70, 114);

    d.printf(
        "%02lu:%02lu:%02lu",
        hours,
        minutes,
        secs
    );

    // Wi-Fi status
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 127);
    d.print("WIFI");

    d.setTextColor(
        WiFi.status() == WL_CONNECTED
            ? GREEN_COLOR
            : YELLOW_COLOR
    );

    d.setCursor(70, 127);

    if (WiFi.status() == WL_CONNECTED) {
        d.print("Connected");
    } else {
        d.print("Not connected");
    }
}

} // namespace

// ====================================
// Public application functions
// ====================================

void systemMonitorStart() {

    Serial.println("Starting System Monitor");

    exitRequested = false;
    lastUpdate = millis();

    drawSystemInfo();
}

void systemMonitorUpdate() {

    // The main OS loop calls M5Cardputer.update().

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {

        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            exitRequested = true;
            return;
        }
    }

    // Refresh every second
    if (millis() - lastUpdate >= 1000) {

        drawSystemInfo();
        lastUpdate = millis();
    }
}

void systemMonitorStop() {

    Serial.println("Stopping System Monitor");

    exitRequested = false;
}

bool systemMonitorWantsExit() {
    return exitRequested;
}
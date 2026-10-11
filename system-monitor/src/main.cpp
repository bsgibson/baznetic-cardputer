#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_system.h>

// ------------------------------------
// BAZNETIC SYSTEM MONITOR v2
// M5Stack Cardputer ADV
// ------------------------------------

#define BG_COLOR      0x0000
#define HEADER_COLOR  0x07FF
#define TEXT_COLOR    0xFFFF
#define GREEN_COLOR   0x07E0
#define YELLOW_COLOR  0xFFE0
#define RED_COLOR     0xF800
#define GREY_COLOR    0x8410

unsigned long lastUpdate = 0;

// ------------------------------------
// Draw header
// ------------------------------------

void drawHeader() {

    M5Cardputer.Display.fillScreen(BG_COLOR);

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(HEADER_COLOR);

    M5Cardputer.Display.setCursor(8, 5);
    M5Cardputer.Display.println("BAZNETIC SYS");

    M5Cardputer.Display.drawLine(
        0, 28, 240, 28, HEADER_COLOR
    );
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

    M5Cardputer.Display.drawRect(
        x, y, width, 9, GREY_COLOR
    );

    int fillWidth = (width - 2) * percent / 100;

    M5Cardputer.Display.fillRect(
        x + 1, y + 1, fillWidth, 7, colour
    );
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

    // --------------------------------
    // CPU frequency
    // --------------------------------

    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 34);
    M5Cardputer.Display.print("CPU");

    M5Cardputer.Display.setTextColor(TEXT_COLOR);
    M5Cardputer.Display.setCursor(70, 34);
    M5Cardputer.Display.printf(
        "%lu MHz",
        (unsigned long)cpuMHz
    );

    // --------------------------------
    // Internal temperature
    // --------------------------------

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 48);
    M5Cardputer.Display.print("TEMP");

    M5Cardputer.Display.setTextColor(
        getTempColor(temperature)
    );

    M5Cardputer.Display.setCursor(70, 48);
    M5Cardputer.Display.printf(
        "%.1f C",
        temperature
    );

    // --------------------------------
    // RAM usage
    // --------------------------------

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 62);
    M5Cardputer.Display.print("RAM");

    M5Cardputer.Display.setTextColor(TEXT_COLOR);
    M5Cardputer.Display.setCursor(70, 62);

    M5Cardputer.Display.printf(
        "%lu / %lu KB",
        (unsigned long)(usedHeap / 1024),
        (unsigned long)(totalHeap / 1024)
    );

    drawBar(70, 74, 150, ramPercent);

    // --------------------------------
    // Flash
    // --------------------------------

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 88);
    M5Cardputer.Display.print("FLASH");

    M5Cardputer.Display.setTextColor(TEXT_COLOR);
    M5Cardputer.Display.setCursor(70, 88);

    M5Cardputer.Display.printf(
        "%lu MB",
        (unsigned long)flashMB
    );

    // --------------------------------
    // Battery
    // --------------------------------

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 101);
    M5Cardputer.Display.print("BATTERY");

    M5Cardputer.Display.setTextColor(TEXT_COLOR);
    M5Cardputer.Display.setCursor(70, 101);

    if (battery >= 0 && battery <= 100) {
        M5Cardputer.Display.printf("%d%%", battery);
    } else {
        M5Cardputer.Display.print("N/A");
    }

    // --------------------------------
    // Uptime
    // --------------------------------

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 114);
    M5Cardputer.Display.print("UPTIME");

    M5Cardputer.Display.setTextColor(TEXT_COLOR);
    M5Cardputer.Display.setCursor(70, 114);

    M5Cardputer.Display.printf(
        "%02lu:%02lu:%02lu",
        hours,
        minutes,
        secs
    );

    // --------------------------------
    // Wi-Fi status
    // --------------------------------

    M5Cardputer.Display.setTextColor(HEADER_COLOR);
    M5Cardputer.Display.setCursor(8, 127);
    M5Cardputer.Display.print("WIFI");

    M5Cardputer.Display.setTextColor(
        WiFi.status() == WL_CONNECTED
            ? GREEN_COLOR
            : YELLOW_COLOR
    );

    M5Cardputer.Display.setCursor(70, 127);

    if (WiFi.status() == WL_CONNECTED) {
        M5Cardputer.Display.print("Connected");
    } else {
        M5Cardputer.Display.print("Not connected");
    }
}

// ------------------------------------
// Setup
// ------------------------------------

void setup() {

    Serial.begin(115200);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(90);

    WiFi.mode(WIFI_STA);

    drawSystemInfo();

    Serial.println(
        "BAZNETIC System Monitor v2 started"
    );
}

// ------------------------------------
// Main loop
// ------------------------------------

void loop() {

    M5Cardputer.update();

    if (millis() - lastUpdate >= 1000) {

        drawSystemInfo();

        lastUpdate = millis();
    }

    delay(20);
}
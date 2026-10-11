#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include "wifi_scanner.h"

namespace {

constexpr uint16_t BG = 0x0000;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t SELECTED = 0x3186;

int networkCount = 0;
int selectedNetwork = 0;
bool detailView = false;
bool exitRequested = false;

constexpr int networksPerPage = 3;

uint16_t signalColor(int rssi) {
    if (rssi >= -60) return GREEN;
    if (rssi >= -75) return YELLOW;
    return RED;
}

void drawHeader(const char* title) {
    auto& d = M5Cardputer.Display;

    d.fillScreen(BG);
    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.println(title);
    d.drawLine(0, 28, 240, 28, CYAN);
}

String securityName(wifi_auth_mode_t type) {
    switch (type) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 ENT";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "OTHER";
    }
}

void drawList() {
    auto& d = M5Cardputer.Display;

    drawHeader("BAZNETIC WiFi");

    d.setTextSize(1);
    d.setTextColor(WHITE);
    d.setCursor(8, 33);
    d.printf("%d networks | R: Rescan", networkCount);

    if (networkCount == 0) {
        d.setTextSize(2);
        d.setCursor(8, 65);
        d.println("No networks");
        return;
    }

    int start = (selectedNetwork / networksPerPage) * networksPerPage;
    int y = 48;

    for (int i = start;
         i < start + networksPerPage && i < networkCount;
         ++i) {

        int rssi = WiFi.RSSI(i);

        if (i == selectedNetwork) {
            d.fillRoundRect(3, y - 2, 234, 28, 4, SELECTED);
        }

        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) ssid = "<Hidden>";
        if (ssid.length() > 18) ssid = ssid.substring(0, 18);

        d.setTextSize(2);
        d.setTextColor(WHITE);
        d.setCursor(8, y);
        d.print(ssid);

        d.setTextSize(1);
        d.setTextColor(signalColor(rssi));
        d.setCursor(10, y + 18);
        d.printf("CH:%d  %d dBm", WiFi.channel(i), rssi);

        y += 29;
    }
}

void drawDetails() {
    if (networkCount == 0) return;

    auto& d = M5Cardputer.Display;
    int i = selectedNetwork;

    drawHeader("NETWORK INFO");

    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) ssid = "<Hidden>";

    d.setTextSize(1);
    d.setTextColor(CYAN);
    d.setCursor(8, 34);
    d.println("SSID:");

    d.setTextSize(2);
    d.setTextColor(WHITE);
    d.setCursor(8, 46);
    d.println(ssid.substring(0, 18));

    d.setTextSize(1);
    d.setCursor(8, 70);
    d.printf("BSSID: %s", WiFi.BSSIDstr(i).c_str());

    d.setCursor(8, 85);
    d.printf("Channel: %d", WiFi.channel(i));

    d.setCursor(8, 100);
    d.printf("Signal: %d dBm", WiFi.RSSI(i));

    d.setCursor(8, 115);
    d.printf("Security: %s",
             securityName(WiFi.encryptionType(i)).c_str());
}

void scanNetworks() {
    detailView = false;
    selectedNetwork = 0;

    WiFi.scanDelete();

    drawHeader("BAZNETIC WiFi");

    auto& d = M5Cardputer.Display;
    d.setTextSize(2);
    d.setTextColor(YELLOW);
    d.setCursor(8, 50);
    d.println("Scanning...");

    networkCount = WiFi.scanNetworks(false, true);

    if (networkCount < 0) networkCount = 0;

    Serial.printf("Wi-Fi networks found: %d\n", networkCount);

    drawList();
}

} // namespace

// ------------------------------------
// Public application functions
// ------------------------------------

void wifiScannerStart() {
    Serial.println("Starting Wi-Fi Scanner");

    exitRequested = false;
    detailView = false;
    selectedNetwork = 0;
    networkCount = 0;

    WiFi.mode(WIFI_STA);
    // Keep any connection managed by Settings intact.

    scanNetworks();
}

void wifiScannerUpdate() {
    // M5Cardputer.update() is called by the OS main loop.

    if (!M5Cardputer.Keyboard.isChange() ||
        !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    auto& keyboard = M5Cardputer.Keyboard;
    Keyboard_Class::KeysState status = keyboard.keysState();

    if (keyboard.isKeyPressed(KEY_BACKSPACE)) {
        if (detailView) {
            detailView = false;
            drawList();
        } else {
            exitRequested = true;
        }
        return;
    }

    if (keyboard.isKeyPressed('r') ||
        keyboard.isKeyPressed('R')) {
        scanNetworks();
        return;
    }

    if (detailView) return;

    if (status.enter) {
        if (networkCount > 0) {
            detailView = true;
            drawDetails();
        }
        return;
    }

    if (keyboard.isKeyPressed(';')) {
        if (selectedNetwork > 0) {
            --selectedNetwork;
            drawList();
        }
    } else if (keyboard.isKeyPressed('.')) {
        if (selectedNetwork < networkCount - 1) {
            ++selectedNetwork;
            drawList();
        }
    }
}

void wifiScannerStop() {
    Serial.println("Stopping Wi-Fi Scanner");

    WiFi.scanDelete();

    networkCount = 0;
    selectedNetwork = 0;
    detailView = false;
    exitRequested = false;

    // Leave Wi-Fi available for future Baznetic OS apps.
}

bool wifiScannerWantsExit() {
    return exitRequested;
}

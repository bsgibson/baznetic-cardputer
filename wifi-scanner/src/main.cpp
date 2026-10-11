#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>

// ------------------------------------
// BAZNETIC Wi-Fi Scanner v2
// M5Stack Cardputer ADV
// ------------------------------------

#define BG_COLOR      0x0000
#define HEADER_COLOR  0x07FF
#define TEXT_COLOR    0xFFFF
#define SIGNAL_GOOD   0x07E0
#define SIGNAL_MED    0xFFE0
#define SIGNAL_WEAK   0xF800
#define SELECT_COLOR  0x3186

int networkCount = 0;
int selectedNetwork = 0;

bool detailView = false;

const int networksPerPage = 3;

// ------------------------------------
// Signal colour
// ------------------------------------

uint16_t getSignalColor(int rssi) {

    if (rssi >= -60) {
        return SIGNAL_GOOD;
    }

    if (rssi >= -75) {
        return SIGNAL_MED;
    }

    return SIGNAL_WEAK;
}

// ------------------------------------
// Header
// ------------------------------------

void drawHeader(const char* title) {

    M5Cardputer.Display.fillScreen(BG_COLOR);

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(HEADER_COLOR);

    M5Cardputer.Display.setCursor(8, 5);
    M5Cardputer.Display.println(title);

    M5Cardputer.Display.drawLine(
        0, 28, 240, 28, HEADER_COLOR
    );
}

// ------------------------------------
// Scan networks
// ------------------------------------

void scanNetworks() {

    detailView = false;
    selectedNetwork = 0;

    WiFi.scanDelete();

    drawHeader("BAZNETIC WiFi");

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(SIGNAL_MED);
    M5Cardputer.Display.setCursor(8, 50);
    M5Cardputer.Display.println("Scanning...");

    Serial.println("Starting Wi-Fi scan...");

    networkCount = WiFi.scanNetworks(false, true);

    if (networkCount < 0) {
        networkCount = 0;
    }

    Serial.printf("Networks found: %d\n", networkCount);
}

// ------------------------------------
// Network list
// ------------------------------------

void drawNetworkList() {

    drawHeader("BAZNETIC WiFi");

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TEXT_COLOR);

    M5Cardputer.Display.setCursor(8, 33);
    M5Cardputer.Display.printf(
        "%d networks  |  R: Rescan",
        networkCount
    );

    if (networkCount == 0) {

        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setCursor(8, 65);
        M5Cardputer.Display.println("No networks");

        return;
    }

    int page = selectedNetwork / networksPerPage;
    int start = page * networksPerPage;

    int y = 48;

    for (int i = start;
         i < start + networksPerPage && i < networkCount;
         i++) {

        int rssi = WiFi.RSSI(i);

        uint16_t signalColor = getSignalColor(rssi);

        if (i == selectedNetwork) {

            M5Cardputer.Display.fillRoundRect(
                3, y - 2, 234, 28, 4, SELECT_COLOR
            );
        }

        String ssid = WiFi.SSID(i);

        if (ssid.length() == 0) {
            ssid = "<Hidden>";
        }

        if (ssid.length() > 18) {
            ssid = ssid.substring(0, 18);
        }

        // Large SSID
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setTextColor(TEXT_COLOR);
        M5Cardputer.Display.setCursor(8, y);
        M5Cardputer.Display.print(ssid);

        // Signal details
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(signalColor);
        M5Cardputer.Display.setCursor(10, y + 18);

        M5Cardputer.Display.printf(
            "CH:%d  %d dBm",
            WiFi.channel(i),
            rssi
        );

        y += 29;
    }
}

// ------------------------------------
// Security type
// ------------------------------------

String getSecurityType(wifi_auth_mode_t type) {

    switch (type) {

        case WIFI_AUTH_OPEN:
            return "OPEN";

        case WIFI_AUTH_WEP:
            return "WEP";

        case WIFI_AUTH_WPA_PSK:
            return "WPA";

        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";

        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";

        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2 ENT";

        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";

        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3";

        default:
            return "OTHER";
    }
}

// ------------------------------------
// Network details
// ------------------------------------

void drawNetworkDetails() {

    if (networkCount == 0) {
        return;
    }

    drawHeader("NETWORK INFO");

    int i = selectedNetwork;

    String ssid = WiFi.SSID(i);

    if (ssid.length() == 0) {
        ssid = "<Hidden>";
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(HEADER_COLOR);

    M5Cardputer.Display.setCursor(8, 34);
    M5Cardputer.Display.println("SSID:");

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TEXT_COLOR);

    M5Cardputer.Display.setCursor(8, 46);
    M5Cardputer.Display.println(ssid.substring(0, 18));

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TEXT_COLOR);

    M5Cardputer.Display.setCursor(8, 70);
    M5Cardputer.Display.printf(
        "BSSID: %s",
        WiFi.BSSIDstr(i).c_str()
    );

    M5Cardputer.Display.setCursor(8, 85);
    M5Cardputer.Display.printf(
        "Channel: %d",
        WiFi.channel(i)
    );

    M5Cardputer.Display.setCursor(8, 100);
    M5Cardputer.Display.printf(
        "Signal: %d dBm",
        WiFi.RSSI(i)
    );

    M5Cardputer.Display.setCursor(8, 115);
    M5Cardputer.Display.printf(
        "Security: %s",
        getSecurityType(WiFi.encryptionType(i)).c_str()
    );
}

// ------------------------------------
// Setup
// ------------------------------------

void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==========================");
    Serial.println("BAZNETIC Wi-Fi Scanner v2");
    Serial.println("==========================");

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // Board detection diagnostics
    int detectedBoard = (int)M5.getBoard();
    int expectedBoard = (int)m5::board_t::board_M5CardputerADV;

    Serial.printf(
        "Detected board type: %d\n",
        detectedBoard
    );

    Serial.printf(
        "Expected Cardputer ADV: %d\n",
        expectedBoard
    );

    if (detectedBoard == expectedBoard) {
        Serial.println("Board detection: OK");
    } else {
        Serial.println("WARNING: Board detection mismatch!");
    }

    // Display setup
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(90);

    // Wi-Fi setup
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    delay(500);

    scanNetworks();
    drawNetworkList();
}

// ------------------------------------
// Main loop
// ------------------------------------

void loop() {

    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {

        Keyboard_Class::KeysState status =
            M5Cardputer.Keyboard.keysState();

        Serial.println("Keyboard event detected!");

        // Rescan
        if (M5Cardputer.Keyboard.isKeyPressed('r') ||
            M5Cardputer.Keyboard.isKeyPressed('R')) {

            Serial.println("Rescan requested");

            scanNetworks();
            drawNetworkList();
        }

        // Return to list
        else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {

            Serial.println("Back to list");

            detailView = false;
            drawNetworkList();
        }

        // Open details
        else if (status.enter) {

            Serial.println("Enter pressed");

            if (networkCount > 0) {
                detailView = true;
                drawNetworkDetails();
            }
        }

        // Navigate
        else if (!detailView) {

            if (M5Cardputer.Keyboard.isKeyPressed(';')) {

                Serial.println("Navigate UP");

                if (selectedNetwork > 0) {
                    selectedNetwork--;
                }

                drawNetworkList();
            }

            else if (M5Cardputer.Keyboard.isKeyPressed('.')) {

                Serial.println("Navigate DOWN");

                if (selectedNetwork < networkCount - 1) {
                    selectedNetwork++;
                }

                drawNetworkList();
            }
        }
    }

    delay(20);
}
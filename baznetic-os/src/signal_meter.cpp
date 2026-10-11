#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include "signal_meter.h"

// ====================================
// BAZNETIC Wi-Fi Signal Meter v0.3
// M5Stack Cardputer ADV
// ====================================

namespace {

constexpr uint16_t BG       = 0x0000;
constexpr uint16_t CYAN     = 0x07FF;
constexpr uint16_t WHITE    = 0xFFFF;
constexpr uint16_t GREEN    = 0x07E0;
constexpr uint16_t YELLOW   = 0xFFE0;
constexpr uint16_t RED      = 0xF800;
constexpr uint16_t GREY     = 0x8410;
constexpr uint16_t SELECTED = 0x3186;

constexpr int MAX_HISTORY = 48;
constexpr int NETWORKS_PER_PAGE = 3;
constexpr unsigned long SCAN_INTERVAL = 3000;

enum Screen {
    NETWORK_LIST,
    SIGNAL_MONITOR
};

Screen currentScreen = NETWORK_LIST;

int networkCount = 0;
int selectedNetwork = 0;

String targetSSID;
String targetBSSID;
int targetChannel = 0;
int currentRSSI = -100;

int history[MAX_HISTORY];
int historyCount = 0;

bool exitRequested = false;
bool scanRunning = false;
bool scanForMonitor = false;

unsigned long lastScan = 0;

// ------------------------------------
// Signal helpers
// ------------------------------------

uint16_t signalColor(int rssi) {
    if (rssi >= -60) return GREEN;
    if (rssi >= -75) return YELLOW;
    return RED;
}

const char* signalQuality(int rssi) {
    if (rssi >= -50) return "Excellent";
    if (rssi >= -60) return "Very Good";
    if (rssi >= -70) return "Good";
    if (rssi >= -80) return "Fair";
    return "Weak";
}

int signalPercent(int rssi) {
    return constrain((rssi + 100) * 2, 0, 100);
}

// ------------------------------------
// Display helpers
// ------------------------------------

void drawHeader(const char* title) {
    auto& d = M5Cardputer.Display;

    d.fillScreen(BG);
    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.println(title);
    d.drawLine(0, 28, 240, 28, CYAN);
}

void drawNetworkList() {
    auto& d = M5Cardputer.Display;

    drawHeader("SIGNAL METER");

    d.setTextSize(1);
    d.setTextColor(WHITE);
    d.setCursor(8, 33);

    d.printf(
        "%d APs  |  R: Rescan",
        networkCount
    );

    if (scanRunning) {
        d.setTextColor(YELLOW);
        d.setCursor(8, 115);
        d.print("Scanning...");
    }

    if (networkCount == 0) {
        d.setTextSize(2);
        d.setTextColor(YELLOW);
        d.setCursor(8, 65);
        d.println("No networks");
        return;
    }

    int start =
        (selectedNetwork / NETWORKS_PER_PAGE)
        * NETWORKS_PER_PAGE;

    int y = 48;

    for (int i = start;
         i < start + NETWORKS_PER_PAGE &&
         i < networkCount;
         i++) {

        int rssi = WiFi.RSSI(i);

        if (i == selectedNetwork) {
            d.fillRoundRect(
                3, y - 2, 234, 28, 4, SELECTED
            );
        }

        String ssid = WiFi.SSID(i);

        if (ssid.isEmpty()) {
            ssid = "<Hidden>";
        }

        if (ssid.length() > 18) {
            ssid = ssid.substring(0, 18);
        }

        d.setTextSize(2);
        d.setTextColor(WHITE);
        d.setCursor(8, y);
        d.print(ssid);

        d.setTextSize(1);
        d.setTextColor(signalColor(rssi));
        d.setCursor(10, y + 18);

        d.printf(
            "CH:%d  %d dBm",
            WiFi.channel(i),
            rssi
        );

        y += 29;
    }
}

// ------------------------------------
// History
// ------------------------------------

void addHistory(int rssi) {

    if (historyCount < MAX_HISTORY) {
        history[historyCount++] = rssi;
        return;
    }

    for (int i = 1; i < MAX_HISTORY; i++) {
        history[i - 1] = history[i];
    }

    history[MAX_HISTORY - 1] = rssi;
}

// ------------------------------------
// Live graph
// ------------------------------------

void drawGraph() {
    auto& d = M5Cardputer.Display;

    const int graphX = 8;
    const int graphY = 91;
    const int graphW = 224;
    const int graphH = 35;

    d.drawRect(
        graphX, graphY,
        graphW, graphH,
        GREY
    );

    // Reference line: -70 dBm
    int referenceY = graphY + 1 +
        ((-30 - (-70)) * (graphH - 2) / 70);

    d.drawFastHLine(
        graphX + 1,
        referenceY,
        graphW - 2,
        GREY
    );

    if (historyCount < 2) return;

    for (int i = 1; i < historyCount; i++) {

        int x1 = graphX + 2 +
            (i - 1) * (graphW - 5) /
            (MAX_HISTORY - 1);

        int x2 = graphX + 2 +
            i * (graphW - 5) /
            (MAX_HISTORY - 1);

        int rssi1 = constrain(history[i - 1], -100, -30);
        int rssi2 = constrain(history[i], -100, -30);

        int y1 = graphY + 1 +
            (-30 - rssi1) * (graphH - 3) / 70;

        int y2 = graphY + 1 +
            (-30 - rssi2) * (graphH - 3) / 70;

        d.drawLine(
            x1, y1,
            x2, y2,
            signalColor(history[i])
        );
    }
}

// ------------------------------------
// Signal monitor screen
// ------------------------------------

void drawSignalMonitor() {
    auto& d = M5Cardputer.Display;

    drawHeader("SIGNAL METER");

    String ssid = targetSSID;

    if (ssid.isEmpty()) {
        ssid = "<Hidden>";
    }

    if (ssid.length() > 22) {
        ssid = ssid.substring(0, 22);
    }

    d.setTextSize(1);
    d.setTextColor(WHITE);

    d.setCursor(8, 32);
    d.printf("SSID: %s", ssid.c_str());

    d.setCursor(8, 44);
    d.printf(
        "CH:%d  AP:%s",
        targetChannel,
        targetBSSID.substring(9).c_str()
    );

    d.setTextSize(2);
    d.setTextColor(signalColor(currentRSSI));
    d.setCursor(8, 58);

    if (currentRSSI <= -100) {
        d.print("Signal lost");
    } else {
        d.printf("%d dBm", currentRSSI);
    }

    d.setTextSize(1);
    d.setCursor(145, 63);
    d.print(signalQuality(currentRSSI));

    // Signal bar
    d.drawRect(8, 79, 224, 9, GREY);

    int barWidth =
        222 * signalPercent(currentRSSI) / 100;

    d.fillRect(
        9, 80,
        barWidth, 7,
        signalColor(currentRSSI)
    );

    drawGraph();

    d.setTextSize(1);
    d.setTextColor(GREY);
    d.setCursor(8, 128);
    d.print("BACK: Networks");
}

// ------------------------------------
// Start asynchronous scan
// ------------------------------------

void startScan(bool monitoring) {

    if (scanRunning) return;

    // Discard previous scan results only
    // when starting a new scan.
    WiFi.scanDelete();

    scanForMonitor = monitoring;
    scanRunning = true;

    int result = WiFi.scanNetworks(
        true,   // Async
        true    // Include hidden networks
    );

    if (result == WIFI_SCAN_FAILED) {
        scanRunning = false;
        Serial.println("Signal Meter: scan failed");
    }

    lastScan = millis();
}

// ------------------------------------
// Process scan results
// ------------------------------------

void processScan() {

    if (!scanRunning) return;

    int result = WiFi.scanComplete();

    if (result == WIFI_SCAN_RUNNING) {
        return;
    }

    scanRunning = false;

    if (result < 0) {
        Serial.printf(
            "Signal Meter: scan error %d\n",
            result
        );
        return;
    }

    if (!scanForMonitor) {

        networkCount = result;
        selectedNetwork = 0;

        drawNetworkList();

    } else {

        bool found = false;

        for (int i = 0; i < result; i++) {

            if (WiFi.BSSIDstr(i).equalsIgnoreCase(
                    targetBSSID)) {

                currentRSSI = WiFi.RSSI(i);
                targetChannel = WiFi.channel(i);

                found = true;
                break;
            }
        }

        if (!found) {
            currentRSSI = -100;
        }

        addHistory(currentRSSI);
        drawSignalMonitor();
    }

    // Keep results available for network selection.
    // The next scan clears them before starting.
}

// ------------------------------------
// Select access point
// ------------------------------------

void selectNetwork() {

    if (networkCount <= 0 ||
        selectedNetwork >= networkCount) {
        return;
    }

    targetSSID = WiFi.SSID(selectedNetwork);
    targetBSSID = WiFi.BSSIDstr(selectedNetwork);
    targetChannel = WiFi.channel(selectedNetwork);
    currentRSSI = WiFi.RSSI(selectedNetwork);

    historyCount = 0;
    addHistory(currentRSSI);

    currentScreen = SIGNAL_MONITOR;

    drawSignalMonitor();

    // Begin first live refresh.
    startScan(true);
}

} // namespace

// ====================================
// Public application functions
// ====================================

void signalMeterStart() {

    Serial.println("Starting Signal Meter");

    exitRequested = false;
    scanRunning = false;
    networkCount = 0;
    selectedNetwork = 0;
    historyCount = 0;
    currentScreen = NETWORK_LIST;

    WiFi.mode(WIFI_STA);

    drawNetworkList();
    startScan(false);
}

void signalMeterUpdate() {

    processScan();

    // Refresh monitor periodically.
    if (currentScreen == SIGNAL_MONITOR &&
        !scanRunning &&
        millis() - lastScan >= SCAN_INTERVAL) {

        startScan(true);
    }

    if (!M5Cardputer.Keyboard.isChange() ||
        !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    auto& keyboard = M5Cardputer.Keyboard;
    Keyboard_Class::KeysState status =
        keyboard.keysState();

    // Backspace
    if (keyboard.isKeyPressed(KEY_BACKSPACE)) {

        if (currentScreen == SIGNAL_MONITOR) {

            // Cancel any outstanding async scan.
            if (scanRunning) {
                WiFi.scanDelete();
                scanRunning = false;
            }

            currentScreen = NETWORK_LIST;
            networkCount = 0;

            drawNetworkList();
            startScan(false);

        } else {

            exitRequested = true;
        }

        return;
    }

    // Rescan list
    if (currentScreen == NETWORK_LIST &&
        (keyboard.isKeyPressed('r') ||
         keyboard.isKeyPressed('R'))) {

        if (!scanRunning) {
            drawNetworkList();
            startScan(false);
        }

        return;
    }

    if (currentScreen != NETWORK_LIST ||
        scanRunning) {
        return;
    }

    // Select
    if (status.enter) {
        selectNetwork();
        return;
    }

    // Up
    if (keyboard.isKeyPressed(';')) {

        if (selectedNetwork > 0) {
            selectedNetwork--;
            drawNetworkList();
        }
    }

    // Down
    else if (keyboard.isKeyPressed('.')) {

        if (selectedNetwork < networkCount - 1) {
            selectedNetwork++;
            drawNetworkList();
        }
    }
}

void signalMeterStop() {

    Serial.println("Stopping Signal Meter");

    if (scanRunning) {
        WiFi.scanDelete();
    }

    scanRunning = false;
    networkCount = 0;
    historyCount = 0;
    exitRequested = false;
}

bool signalMeterWantsExit() {
    return exitRequested;
}
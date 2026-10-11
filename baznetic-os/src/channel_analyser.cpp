#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <math.h>

#include "channel_analyser.h"

// ====================================
// BAZNETIC CHANNEL ANALYSER v0.5
// M5Stack Cardputer ADV
// ====================================

namespace {

constexpr uint16_t BG     = 0x0000;
constexpr uint16_t CYAN   = 0x07FF;
constexpr uint16_t WHITE  = 0xFFFF;
constexpr uint16_t GREEN  = 0x07E0;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t RED    = 0xF800;
constexpr uint16_t GREY   = 0x8410;

constexpr int FIRST_CHANNEL = 1;
constexpr int LAST_CHANNEL = 13;

constexpr unsigned long SCAN_INTERVAL = 10000;

// These scores are estimates based on detected
// access points, RSSI and channel separation.
// They are NOT measured airtime utilisation.

float channelScore[14] = {};
int channelAPs[14] = {};

int totalAPs = 0;
int bestChannel = 1;

bool scanRunning = false;
bool exitRequested = false;
bool hasResults = false;

unsigned long lastScan = 0;

// ------------------------------------
// Display helpers
// ------------------------------------

void header() {

    auto& d = M5Cardputer.Display;

    d.fillScreen(BG);
    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.print("CHANNELS");

    d.drawLine(0, 28, 240, 28, CYAN);
}

// ------------------------------------
// Approximate overlap weighting
// ------------------------------------

float overlapWeight(int distance) {

    // Approximation for 20 MHz networks
    // separated by 5 MHz per channel.

    switch (distance) {
        case 0: return 1.00f;
        case 1: return 0.80f;
        case 2: return 0.60f;
        case 3: return 0.35f;
        case 4: return 0.15f;
        default: return 0.0f;
    }
}

// ------------------------------------
// Calculate channel scores
// ------------------------------------

void calculateScores(int count) {

    for (int ch = 1; ch <= 13; ch++) {
        channelScore[ch] = 0;
        channelAPs[ch] = 0;
    }

    totalAPs = count;

    for (int i = 0; i < count; i++) {

        int apChannel = WiFi.channel(i);
        int rssi = WiFi.RSSI(i);

        if (apChannel < 1 || apChannel > 13) {
            continue;
        }

        channelAPs[apChannel]++;

        // Stronger APs contribute more.
        // A -50 dBm AP contributes more
        // than a -85 dBm AP.

        float strength =
            constrain((rssi + 100) / 50.0f,
                      0.1f, 1.0f);

        for (int ch = 1; ch <= 13; ch++) {

            int distance = abs(ch - apChannel);

            channelScore[ch] +=
                strength * overlapWeight(distance);
        }
    }

    // Prefer the usual non-overlapping
    // 20 MHz channel choices: 1, 6, 11.
    //
    // This is a simple recommendation,
    // not a guarantee of best performance.

    const int candidates[] = {1, 6, 11};

    bestChannel = candidates[0];

    for (int i = 1; i < 3; i++) {

        int candidate = candidates[i];

        if (channelScore[candidate] <
            channelScore[bestChannel]) {

            bestChannel = candidate;
        }
    }
}

// ------------------------------------
// Colour by relative channel score
// ------------------------------------

uint16_t scoreColor(float score, float maximum) {

    if (maximum <= 0.0f) {
        return GREEN;
    }

    float ratio = score / maximum;

    if (ratio < 0.35f) return GREEN;
    if (ratio < 0.70f) return YELLOW;

    return RED;
}

// ------------------------------------
// Draw channel chart
// ------------------------------------

void drawChart() {

    auto& d = M5Cardputer.Display;

    header();

    d.setTextSize(1);
    d.setTextColor(WHITE);
    d.setCursor(8, 32);

    d.printf("%d APs detected", totalAPs);

    if (scanRunning) {
        d.setTextColor(YELLOW);
        d.setCursor(165, 32);
        d.print("Scanning");
    }

    if (!hasResults) {

        d.setTextColor(YELLOW);
        d.setCursor(8, 60);
        d.print("Scanning nearby Wi-Fi...");

        d.setTextColor(GREY);
        d.setCursor(8, 125);
        d.print("BACK: Menu");

        return;
    }

    float maximum = 0.0f;

    for (int ch = FIRST_CHANNEL;
         ch <= LAST_CHANNEL; ch++) {

        if (channelScore[ch] > maximum) {
            maximum = channelScore[ch];
        }
    }

    // 13 horizontal rows fit into the
    // 240 x 135 Cardputer display.

    const int rowY = 42;
    const int rowHeight = 5;
    const int rowGap = 1;

    const int barX = 42;
    const int barWidth = 140;

    for (int ch = FIRST_CHANNEL;
         ch <= LAST_CHANNEL; ch++) {

        int y = rowY +
            (ch - FIRST_CHANNEL) *
            (rowHeight + rowGap);

        d.setTextSize(1);

        if (ch == bestChannel) {
            d.setTextColor(GREEN);
        } else {
            d.setTextColor(WHITE);
        }

        d.setCursor(5, y - 1);
        d.printf("%02d", ch);

        int width = 0;

        if (maximum > 0.0f) {
            width = (int)(
                channelScore[ch] /
                maximum * barWidth
            );
        }

        d.fillRect(
            barX, y,
            width, rowHeight,
            scoreColor(channelScore[ch], maximum)
        );

        d.setTextColor(GREY);
        d.setCursor(188, y - 1);

        d.printf("%d", channelAPs[ch]);

        if (ch == bestChannel) {
            d.setTextColor(GREEN);
            d.setCursor(212, y - 1);
            d.print("*");
        }
    }

    d.setTextColor(GREEN);
    d.setCursor(8, 125);
    d.printf("Best: CH%d", bestChannel);

    d.setTextColor(GREY);
    d.setCursor(120, 125);
    d.print("R:Scan BACK:Exit");
}

// ------------------------------------
// Start asynchronous scan
// ------------------------------------

void startScan() {

    if (scanRunning) return;

    WiFi.scanDelete();

    scanRunning = true;
    lastScan = millis();

    int result = WiFi.scanNetworks(
        true,   // Asynchronous
        true    // Include hidden SSIDs
    );

    if (result == WIFI_SCAN_FAILED) {
        scanRunning = false;
        Serial.println("Channel scan failed");
    }

    drawChart();
}

// ------------------------------------
// Process completed scan
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
            "Channel scan error: %d\n",
            result
        );

        drawChart();
        return;
    }

    calculateScores(result);

    hasResults = true;

    Serial.printf(
        "Channel scan: %d APs, best CH%d\n",
        totalAPs,
        bestChannel
    );

    drawChart();

    WiFi.scanDelete();
}

} // namespace

// ====================================
// Public application functions
// ====================================

void channelAnalyserStart() {

    Serial.println("Starting Channel Analyser");

    exitRequested = false;
    scanRunning = false;
    hasResults = false;

    totalAPs = 0;
    bestChannel = 1;

    WiFi.mode(WIFI_STA);

    drawChart();
    startScan();
}

void channelAnalyserUpdate() {

    processScan();

    if (!scanRunning &&
        millis() - lastScan >= SCAN_INTERVAL) {

        startScan();
    }

    if (!M5Cardputer.Keyboard.isChange() ||
        !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    auto& keyboard = M5Cardputer.Keyboard;

    if (keyboard.isKeyPressed(KEY_BACKSPACE)) {
        exitRequested = true;
        return;
    }

    if (keyboard.isKeyPressed('r') ||
        keyboard.isKeyPressed('R')) {

        if (!scanRunning) {
            startScan();
        }
    }
}

void channelAnalyserStop() {

    Serial.println("Stopping Channel Analyser");

    WiFi.scanDelete();

    scanRunning = false;
    exitRequested = false;
}

bool channelAnalyserWantsExit() {
    return exitRequested;
}
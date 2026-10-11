#include <Arduino.h>
#include <M5Cardputer.h>

#include "wifi_scanner.h"
#include "system_monitor.h"
#include "signal_meter.h"
#include "channel_analyser.h"
#include "settings.h"
#include "wifi_manager.h"
#include "screensaver.h"

// ====================================
// BAZNETIC OS v0.6
// M5Stack Cardputer ADV
// ====================================

#define BG_COLOR       0x0000
#define HEADER_COLOR   0x07FF
#define TEXT_COLOR     0xFFFF
#define SELECT_COLOR   0x3186
#define GREEN_COLOR    0x07E0
#define GREY_COLOR     0x8410

// ====================================
// Applications
// ====================================

enum AppID {
    APP_LAUNCHER,
    APP_WIFI_SCANNER,
    APP_SYSTEM_MONITOR,
    APP_SIGNAL_METER,
    APP_CHANNEL_ANALYSER,
    APP_SETTINGS,
    APP_SCREENSAVER
};

AppID currentApp = APP_LAUNCHER;

// ====================================
// Launcher menu
// ====================================

const char* menuItems[] = {
    "Wi-Fi Scanner",
    "System Monitor",
    "Wi-Fi Signal Meter",
    "Channel Analyser",
    "Settings"
};

constexpr int menuCount =
    sizeof(menuItems) / sizeof(menuItems[0]);

constexpr int VISIBLE_ITEMS = 4;

int selectedMenu = 0;
int menuScroll = 0;
unsigned long launcherLastActivity = 0;

// ====================================
// Header
// ====================================

void drawHeader(const char* title) {

    auto& d = M5Cardputer.Display;

    d.fillScreen(BG_COLOR);

    d.setTextSize(2);
    d.setTextColor(HEADER_COLOR);
    d.setCursor(8, 5);
    d.println(title);

    d.drawLine(0, 28, 240, 28, HEADER_COLOR);
}

// ====================================
// Boot splash
// ====================================

void showSplash() {

    auto& d = M5Cardputer.Display;

    d.fillScreen(BG_COLOR);

    d.setTextSize(3);
    d.setTextColor(HEADER_COLOR);
    d.setCursor(20, 25);
    d.println("BAZNETIC");

    d.setTextSize(2);
    d.setTextColor(TEXT_COLOR);
    d.setCursor(80, 65);
    d.println("OS");

    d.setTextSize(1);
    d.setTextColor(GREEN_COLOR);
    d.setCursor(85, 100);
    d.println("Version 0.6");

    delay(1500);
}

// ====================================
// Scrolling launcher
// ====================================

void drawLauncher() {

    auto& d = M5Cardputer.Display;

    drawHeader("BAZNETIC OS");

    // Battery indicator
    int battery = M5Cardputer.Power.getBatteryLevel();

    d.setTextSize(1);
    d.setTextColor(GREEN_COLOR);

    if (battery >= 0 && battery <= 100) {
        d.setCursor(200, 33);
        d.printf("%d%%", battery);
    }

    d.setTextColor(GREY_COLOR);
    d.setCursor(8, 33);
    d.print("Cardputer ADV");

    // Ensure selected application is visible
    if (selectedMenu < menuScroll) {
        menuScroll = selectedMenu;
    }

    if (selectedMenu >= menuScroll + VISIBLE_ITEMS) {
        menuScroll = selectedMenu - VISIBLE_ITEMS + 1;
    }

    // Draw visible menu items
    for (int row = 0; row < VISIBLE_ITEMS; row++) {

        int index = menuScroll + row;

        if (index >= menuCount) {
            break;
        }

        int y = 49 + row * 19;

        if (index == selectedMenu) {

            d.fillRoundRect(
                4, y - 3, 232, 19, 4,
                SELECT_COLOR
            );

            d.setTextColor(GREEN_COLOR);
            d.setCursor(9, y);
            d.print(">");

        } else {

            d.setTextColor(TEXT_COLOR);
        }

        d.setTextSize(1);
        d.setCursor(23, y);
        d.print(menuItems[index]);
    }

    // Scroll indicator
    if (menuScroll > 0) {
        d.setTextColor(GREEN_COLOR);
        d.setCursor(230, 32);
        d.print("^");
    }

    if (menuScroll + VISIBLE_ITEMS < menuCount) {
        d.setTextColor(GREEN_COLOR);
        d.setCursor(230, 116);
        d.print("v");
    }

    // Footer
    d.drawLine(0, 124, 240, 124, HEADER_COLOR);

    d.setTextSize(1);
    d.setTextColor(GREY_COLOR);

    d.setCursor(8, 126);
    d.print("v0.6");

    d.setCursor(55, 126);
    d.print("S:Saver   ENTER:Select");
}

// ====================================
// Launch selected application
// ====================================

void launchApplication() {

    // ESP32 cannot reliably scan while association is in progress.
    if ((selectedMenu == 0 || selectedMenu == 2 || selectedMenu == 3) &&
        wifiManagerIsConnecting()) {
        M5Cardputer.Display.fillRect(8, 33, 180, 10, BG_COLOR);
        M5Cardputer.Display.setCursor(8, 33);
        M5Cardputer.Display.setTextColor(GREEN_COLOR);
        M5Cardputer.Display.print("Wi-Fi connecting; try again");
        return;
    }

    switch (selectedMenu) {

        case 0:
            currentApp = APP_WIFI_SCANNER;
            wifiScannerStart();
            break;

        case 1:
            currentApp = APP_SYSTEM_MONITOR;
            systemMonitorStart();
            break;

        case 2:
            currentApp = APP_SIGNAL_METER;
            signalMeterStart();
            break;

        case 3:
            currentApp = APP_CHANNEL_ANALYSER;
            channelAnalyserStart();
            break;

        case 4:
            currentApp = APP_SETTINGS;
            settingsStart();
            break;

        default:
            currentApp = APP_LAUNCHER;
            return;
    }

    Serial.printf(
        "Launching application: %s\n",
        menuItems[selectedMenu]
    );
}

// ====================================
// Return to launcher
// ====================================

void returnToLauncher() {

    switch (currentApp) {

        case APP_WIFI_SCANNER:
            wifiScannerStop();
            break;

        case APP_SYSTEM_MONITOR:
            systemMonitorStop();
            break;

        case APP_SIGNAL_METER:
            signalMeterStop();
            break;

        case APP_CHANNEL_ANALYSER:
            channelAnalyserStop();
            break;

        case APP_SCREENSAVER:
            screensaverStop();
            break;

        case APP_SETTINGS:
            settingsStop();
            break;

        default:
            break;
    }

    currentApp = APP_LAUNCHER;
    launcherLastActivity = millis();

    drawLauncher();

    Serial.println("Returned to Baznetic OS");
}

// ====================================
// Setup
// ====================================

void setup() {

    Serial.begin(115200);

    auto cfg = M5.config();

    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);

    // Restore saved brightness
    settingsInit();
    screensaverInit();
    wifiManagerInit();

    Serial.println();
    Serial.println("======================");
    Serial.println("BAZNETIC OS v0.6");
    Serial.println("======================");

    showSplash();
    launcherLastActivity = millis();
    drawLauncher();
}

// ====================================
// Main loop
// ====================================

void loop() {

    // Update keyboard once per loop
    M5Cardputer.update();
    wifiManagerTick(currentApp == APP_LAUNCHER ||
                    currentApp == APP_SYSTEM_MONITOR ||
                    currentApp == APP_SETTINGS ||
                    currentApp == APP_SCREENSAVER);

    if (currentApp == APP_SCREENSAVER) {
        screensaverUpdate();
        if (screensaverWantsExit()) returnToLauncher();
        delay(20);
        return;
    }

    // --------------------------------
    // Wi-Fi Scanner
    // --------------------------------

    if (currentApp == APP_WIFI_SCANNER) {

        wifiScannerUpdate();

        if (wifiScannerWantsExit()) {
            returnToLauncher();
        }

        delay(20);
        return;
    }

    // --------------------------------
    // System Monitor
    // --------------------------------

    if (currentApp == APP_SYSTEM_MONITOR) {

        systemMonitorUpdate();

        if (systemMonitorWantsExit()) {
            returnToLauncher();
        }

        delay(20);
        return;
    }

    // --------------------------------
    // Wi-Fi Signal Meter
    // --------------------------------

    if (currentApp == APP_SIGNAL_METER) {

        signalMeterUpdate();

        if (signalMeterWantsExit()) {
            returnToLauncher();
        }

        delay(20);
        return;
    }

    // --------------------------------
    // Channel Analyser
    // --------------------------------

    if (currentApp == APP_CHANNEL_ANALYSER) {

        channelAnalyserUpdate();

        if (channelAnalyserWantsExit()) {
            returnToLauncher();
        }

        delay(20);
        return;
    }

    // --------------------------------
    // Settings
    // --------------------------------

    if (currentApp == APP_SETTINGS) {

        settingsUpdate();

        if (settingsWantsExit()) {
            returnToLauncher();
        }

        delay(20);
        return;
    }

    // Auto-start only on the idle launcher; never interrupt an app.
    if (M5Cardputer.Keyboard.isPressed()) launcherLastActivity = millis();
    unsigned long idleTimeout = screensaverIdleTimeoutMs();
    if (idleTimeout > 0 && millis() - launcherLastActivity >= idleTimeout) {
        currentApp = APP_SCREENSAVER;
        screensaverStart();
        return;
    }

    // --------------------------------
    // Launcher keyboard
    // --------------------------------

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {

        auto& keyboard = M5Cardputer.Keyboard;

        Keyboard_Class::KeysState status =
            keyboard.keysState();

        if (keyboard.isKeyPressed('s') || keyboard.isKeyPressed('S')) {
            currentApp = APP_SCREENSAVER;
            screensaverStart();
            return;
        }

        // Up
        if (keyboard.isKeyPressed(';')) {

            selectedMenu--;

            if (selectedMenu < 0) {
                selectedMenu = menuCount - 1;
            }

            drawLauncher();
        }

        // Down
        else if (keyboard.isKeyPressed('.')) {

            selectedMenu++;

            if (selectedMenu >= menuCount) {
                selectedMenu = 0;
            }

            drawLauncher();
        }

        // Select
        else if (status.enter) {
            launchApplication();
        }
    }

    delay(20);
}

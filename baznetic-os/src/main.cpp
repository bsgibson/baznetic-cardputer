#include <Arduino.h>
#include <M5Cardputer.h>

#include "wifi_scanner.h"
#include "system_monitor.h"

// ====================================
// BAZNETIC OS v0.2
// M5Stack Cardputer ADV
// ====================================

#define BG_COLOR       0x0000
#define HEADER_COLOR   0x07FF
#define TEXT_COLOR     0xFFFF
#define SELECT_COLOR   0x3186
#define GREEN_COLOR    0x07E0
#define YELLOW_COLOR   0xFFE0
#define GREY_COLOR     0x8410

// ====================================
// Application IDs
// ====================================

enum AppID {
    APP_LAUNCHER,
    APP_WIFI_SCANNER,
    APP_SYSTEM_MONITOR,
    APP_SIGNAL_METER,
    APP_SETTINGS
};

AppID currentApp = APP_LAUNCHER;

// ====================================
// Menu configuration
// ====================================

const char* menuItems[] = {
    "Wi-Fi Scanner",
    "System Monitor",
    "Wi-Fi Signal Meter",
    "Settings"
};

const int menuCount = 4;
int selectedMenu = 0;

// ====================================
// Draw header
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
// Boot splash screen
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
    d.println("Version 0.2");

    delay(1500);
}

// ====================================
// Draw launcher
// ====================================

void drawLauncher() {

    drawHeader("BAZNETIC OS");

    auto& d = M5Cardputer.Display;

    d.setTextSize(1);
    d.setTextColor(GREY_COLOR);
    d.setCursor(8, 33);
    d.println("Cardputer ADV");

    int y = 49;

    for (int i = 0; i < menuCount; i++) {

        d.setTextSize(1);

        if (i == selectedMenu) {

            d.fillRoundRect(
                4, y - 3, 232, 19, 4, SELECT_COLOR
            );

            d.setTextColor(GREEN_COLOR);
            d.setCursor(9, y);
            d.print(">");

        } else {
            d.setTextColor(TEXT_COLOR);
        }

        d.setCursor(23, y);
        d.println(menuItems[i]);

        y += 19;
    }

    // Footer
    d.drawLine(0, 124, 240, 124, HEADER_COLOR);

    d.setTextSize(1);
    d.setTextColor(GREY_COLOR);

    d.setCursor(8, 126);
    d.print("v0.2");

    d.setCursor(105, 126);
    d.print("ENTER: Select");
}

// ====================================
// Placeholder applications
// ====================================

void drawApplication(AppID app) {

    const char* title = "APPLICATION";

    switch (app) {

        case APP_SIGNAL_METER:
            title = "SIGNAL METER";
            break;

        case APP_SETTINGS:
            title = "SETTINGS";
            break;

        default:
            break;
    }

    drawHeader(title);

    auto& d = M5Cardputer.Display;

    d.setTextSize(2);
    d.setTextColor(YELLOW_COLOR);
    d.setCursor(15, 55);
    d.println("Coming soon!");

    d.setTextSize(1);
    d.setTextColor(GREY_COLOR);
    d.setCursor(15, 100);
    d.println("BACKSPACE: Main Menu");
}

// ====================================
// Launch selected application
// ====================================

void launchApplication() {

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
            drawApplication(currentApp);
            break;

        case 3:
            currentApp = APP_SETTINGS;
            drawApplication(currentApp);
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

        default:
            break;
    }

    currentApp = APP_LAUNCHER;

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
    M5Cardputer.Display.setBrightness(90);

    Serial.println();
    Serial.println("====================");
    Serial.println("BAZNETIC OS v0.2");
    Serial.println("====================");

    if (M5.getBoard() ==
        m5::board_t::board_M5CardputerADV) {

        Serial.println("Cardputer ADV detected");

    } else {

        Serial.println("WARNING: Unexpected board");
    }

    showSplash();
    drawLauncher();
}

// ====================================
// Main loop
// ====================================

void loop() {

    M5Cardputer.update();

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
    // Launcher and placeholder apps
    // --------------------------------

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {

        Keyboard_Class::KeysState status =
            M5Cardputer.Keyboard.keysState();

        // Backspace
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {

            if (currentApp != APP_LAUNCHER) {
                returnToLauncher();
            }
        }

        // Launcher navigation
        else if (currentApp == APP_LAUNCHER) {

            // Up
            if (M5Cardputer.Keyboard.isKeyPressed(';')) {

                selectedMenu--;

                if (selectedMenu < 0) {
                    selectedMenu = menuCount - 1;
                }

                drawLauncher();
            }

            // Down
            else if (M5Cardputer.Keyboard.isKeyPressed('.')) {

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
    }

    delay(20);
}
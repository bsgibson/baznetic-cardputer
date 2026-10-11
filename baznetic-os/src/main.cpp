#include <Arduino.h>
#include <M5Cardputer.h>

// ====================================
// BAZNETIC OS v0.1
// M5Stack Cardputer ADV
// ====================================

// Colours
#define BG_COLOR       0x0000
#define HEADER_COLOR   0x07FF
#define TEXT_COLOR     0xFFFF
#define SELECT_COLOR   0x3186
#define GREEN_COLOR    0x07E0
#define YELLOW_COLOR   0xFFE0
#define GREY_COLOR     0x8410

// Application IDs
enum AppID {
    APP_LAUNCHER,
    APP_WIFI_SCANNER,
    APP_SYSTEM_MONITOR,
    APP_SIGNAL_METER,
    APP_SETTINGS
};

AppID currentApp = APP_LAUNCHER;

// Menu configuration
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

    M5Cardputer.Display.fillScreen(BG_COLOR);

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(HEADER_COLOR);

    M5Cardputer.Display.setCursor(8, 5);
    M5Cardputer.Display.println(title);

    M5Cardputer.Display.drawLine(
        0, 28, 240, 28, HEADER_COLOR
    );
}

// ====================================
// Boot splash screen
// ====================================

void showSplash() {

    M5Cardputer.Display.fillScreen(BG_COLOR);

    // Main title
    M5Cardputer.Display.setTextSize(3);
    M5Cardputer.Display.setTextColor(HEADER_COLOR);

    M5Cardputer.Display.setCursor(20, 25);
    M5Cardputer.Display.println("BAZNETIC");

    // OS title
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TEXT_COLOR);

    M5Cardputer.Display.setCursor(80, 65);
    M5Cardputer.Display.println("OS");

    // Version
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(GREEN_COLOR);

    M5Cardputer.Display.setCursor(85, 100);
    M5Cardputer.Display.println("Version 0.1");

    delay(1500);
}

// ====================================
// Draw launcher
// ====================================

void drawLauncher() {

    drawHeader("BAZNETIC OS");

    // Subtitle
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(GREY_COLOR);

    M5Cardputer.Display.setCursor(8, 33);
    M5Cardputer.Display.println("Cardputer ADV");

    // Menu items
    int y = 49;

    for (int i = 0; i < menuCount; i++) {

        if (i == selectedMenu) {

            M5Cardputer.Display.fillRoundRect(
                4, y - 3, 232, 19, 4, SELECT_COLOR
            );

            M5Cardputer.Display.setTextColor(
                GREEN_COLOR
            );

            M5Cardputer.Display.setCursor(9, y);
            M5Cardputer.Display.print(">");

        } else {

            M5Cardputer.Display.setTextColor(
                TEXT_COLOR
            );
        }

        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(23, y);
        M5Cardputer.Display.println(menuItems[i]);

        y += 19;
    }

    // Footer
    M5Cardputer.Display.drawLine(
        0, 124, 240, 124, HEADER_COLOR
    );

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(GREY_COLOR);

    M5Cardputer.Display.setCursor(8, 126);
    M5Cardputer.Display.print("v0.1");

    M5Cardputer.Display.setCursor(105, 126);
    M5Cardputer.Display.print("ENTER: Select");
}

// ====================================
// Application placeholder
// ====================================

void drawApplication(AppID app) {

    const char* title = "APPLICATION";

    switch (app) {

        case APP_WIFI_SCANNER:
            title = "WIFI SCANNER";
            break;

        case APP_SYSTEM_MONITOR:
            title = "SYSTEM MONITOR";
            break;

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

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(YELLOW_COLOR);

    M5Cardputer.Display.setCursor(15, 55);
    M5Cardputer.Display.println("Coming soon!");

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(GREY_COLOR);

    M5Cardputer.Display.setCursor(15, 100);
    M5Cardputer.Display.println("BACKSPACE: Main Menu");
}

// ====================================
// Launch selected application
// ====================================

void launchApplication() {

    switch (selectedMenu) {

        case 0:
            currentApp = APP_WIFI_SCANNER;
            break;

        case 1:
            currentApp = APP_SYSTEM_MONITOR;
            break;

        case 2:
            currentApp = APP_SIGNAL_METER;
            break;

        case 3:
            currentApp = APP_SETTINGS;
            break;
    }

    Serial.printf(
        "Launching application: %s\n",
        menuItems[selectedMenu]
    );

    drawApplication(currentApp);
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
    Serial.println("BAZNETIC OS v0.1");
    Serial.println("====================");

    // Confirm board detection
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

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {

        Keyboard_Class::KeysState status =
            M5Cardputer.Keyboard.keysState();

        // Return to launcher
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {

            if (currentApp != APP_LAUNCHER) {

                currentApp = APP_LAUNCHER;
                drawLauncher();

                Serial.println("Returned to launcher");
            }
        }

        // Launcher navigation
        else if (currentApp == APP_LAUNCHER) {

            // Move up
            if (M5Cardputer.Keyboard.isKeyPressed(';')) {

                selectedMenu--;

                if (selectedMenu < 0) {
                    selectedMenu = menuCount - 1;
                }

                drawLauncher();
            }

            // Move down
            else if (M5Cardputer.Keyboard.isKeyPressed('.')) {

                selectedMenu++;

                if (selectedMenu >= menuCount) {
                    selectedMenu = 0;
                }

                drawLauncher();
            }

            // Select application
            else if (status.enter) {

                launchApplication();
            }
        }
    }

    delay(20);
}
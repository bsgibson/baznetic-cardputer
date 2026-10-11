#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>

#include "settings.h"

namespace {

constexpr uint16_t BG     = 0x0000;
constexpr uint16_t CYAN   = 0x07FF;
constexpr uint16_t WHITE  = 0xFFFF;
constexpr uint16_t GREEN  = 0x07E0;
constexpr uint16_t GREY   = 0x8410;
constexpr uint16_t SELECT = 0x3186;

constexpr int MIN_BRIGHTNESS = 10;
constexpr int MAX_BRIGHTNESS = 100;
constexpr int BRIGHTNESS_STEP = 10;

Preferences preferences;

int brightness = 90;
int selectedItem = 0;

bool exitRequested = false;

const int MENU_COUNT = 2;

// ------------------------------------
// Apply brightness
// ------------------------------------

void applyBrightness() {
    M5Cardputer.Display.setBrightness(brightness);
}

// ------------------------------------
// Save brightness
// ------------------------------------

void saveBrightness() {

    preferences.begin("baznetic", false);
    preferences.putInt("brightness", brightness);
    preferences.end();

    Serial.printf(
        "Saved brightness: %d\n",
        brightness
    );
}

// ------------------------------------
// Draw settings
// ------------------------------------

void drawSettings() {

    auto& d = M5Cardputer.Display;

    d.fillScreen(BG);

    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.println("SETTINGS");

    d.drawLine(0, 28, 240, 28, CYAN);

    d.setTextSize(1);

    // Brightness
    if (selectedItem == 0) {
        d.fillRoundRect(
            4, 39, 232, 30, 4, SELECT
        );
    }

    d.setTextColor(WHITE);
    d.setCursor(10, 45);
    d.print("Brightness");

    d.setTextColor(GREEN);
    d.setCursor(175, 45);
    d.printf("%d%%", brightness);

    // Brightness bar
    d.drawRect(10, 58, 210, 7, GREY);

    int barWidth = 208 * brightness / 100;

    d.fillRect(
        11, 59,
        barWidth, 5,
        GREEN
    );

    // About
    if (selectedItem == 1) {
        d.fillRoundRect(
            4, 76, 232, 22, 4, SELECT
        );
    }

    d.setTextColor(WHITE);
    d.setCursor(10, 82);
    d.print("About Baznetic OS");

    d.setTextColor(GREY);
    d.setCursor(8, 115);
    d.print("ENTER: Select  BACK: Exit");
}

// ------------------------------------
// Draw About screen
// ------------------------------------

void drawAbout() {

    auto& d = M5Cardputer.Display;

    d.fillScreen(BG);

    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.println("ABOUT");

    d.drawLine(0, 28, 240, 28, CYAN);

    d.setTextSize(1);

    d.setTextColor(WHITE);
    d.setCursor(8, 36);
    d.println("Baznetic OS v0.4");

    d.setCursor(8, 50);
    d.println("M5Stack Cardputer ADV");

    d.setCursor(8, 64);
    d.printf(
        "CPU: %lu MHz",
        (unsigned long)ESP.getCpuFreqMHz()
    );

    d.setCursor(8, 78);
    d.printf(
        "Flash: %lu MB",
        (unsigned long)(
            ESP.getFlashChipSize() / 1048576
        )
    );

    d.setCursor(8, 92);
    d.printf(
        "Battery: %d%%",
        M5Cardputer.Power.getBatteryLevel()
    );

    d.setTextColor(GREY);
    d.setCursor(8, 120);
    d.println("BACK: Settings");
}

bool showingAbout = false;

} // namespace

// ====================================
// Public functions
// ====================================

void settingsInit() {

    preferences.begin("baznetic", true);

    brightness = preferences.getInt(
        "brightness", 90
    );

    preferences.end();

    brightness = constrain(
        brightness,
        MIN_BRIGHTNESS,
        MAX_BRIGHTNESS
    );

    applyBrightness();
}

void settingsStart() {

    exitRequested = false;
    showingAbout = false;
    selectedItem = 0;

    drawSettings();
}

void settingsUpdate() {

    if (!M5Cardputer.Keyboard.isChange() ||
        !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    auto& keyboard = M5Cardputer.Keyboard;

    Keyboard_Class::KeysState status =
        keyboard.keysState();

    // Backspace
    if (keyboard.isKeyPressed(KEY_BACKSPACE)) {

        if (showingAbout) {
            showingAbout = false;
            drawSettings();
        } else {
            exitRequested = true;
        }

        return;
    }

    if (showingAbout) {
        return;
    }

    // Up
    if (keyboard.isKeyPressed(';')) {

        selectedItem--;

        if (selectedItem < 0) {
            selectedItem = MENU_COUNT - 1;
        }

        drawSettings();
    }

    // Down
    else if (keyboard.isKeyPressed('.')) {

        selectedItem++;

        if (selectedItem >= MENU_COUNT) {
            selectedItem = 0;
        }

        drawSettings();
    }

    // Brightness adjustment
    else if (selectedItem == 0 &&
             keyboard.isKeyPressed(',')) {

        brightness = max(
            MIN_BRIGHTNESS,
            brightness - BRIGHTNESS_STEP
        );

        applyBrightness();
        saveBrightness();
        drawSettings();
    }

    else if (selectedItem == 0 &&
             keyboard.isKeyPressed('/')) {

        brightness = min(
            MAX_BRIGHTNESS,
            brightness + BRIGHTNESS_STEP
        );

        applyBrightness();
        saveBrightness();
        drawSettings();
    }

    // Enter
    else if (status.enter) {

        if (selectedItem == 1) {
            showingAbout = true;
            drawAbout();
        }
    }
}

void settingsStop() {
    exitRequested = false;
}

bool settingsWantsExit() {
    return exitRequested;
}
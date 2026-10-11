#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>

#include "settings.h"
#include "wifi_manager.h"
#include "screensaver.h"

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

const int MENU_COUNT = 4;

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
    const char* items[] = {"Wi-Fi Manager", "Brightness", "Screensaver", "About Baznetic OS"};
    const int positions[] = {38, 57, 86, 105};
    for (int i = 0; i < MENU_COUNT; ++i) {
        int y = positions[i];
        if (i == selectedItem) d.fillRoundRect(4, y - 3, 232, i == 1 ? 26 : 18, 4, SELECT);
        d.setTextColor(WHITE);
        d.setCursor(10, y);
        d.print(items[i]);
    }
    d.setTextColor(GREEN);
    d.setCursor(175, 57);
    d.printf("%d%%", brightness);
    d.drawRect(10, 69, 210, 7, GREY);
    d.fillRect(11, 70, 208 * brightness / 100, 5, GREEN);
    d.setTextColor(GREY);
    d.setCursor(8, 126);
    d.print("ENTER:Select  BACK:Exit");
}

int saverSelected = 0;
bool showingSaverSettings = false;
bool showingSaverPreview = false;
String saverMessage;

void drawSaverSettings() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(BG);
    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.print("SCREENSAVER");
    d.drawLine(0, 28, 240, 28, CYAN);
    d.setTextSize(1);
    for (int i = 0; i < 3; ++i) {
        int y = 42 + i * 23;
        if (i == saverSelected) d.fillRoundRect(4, y - 3, 232, 20, 4, SELECT);
        d.setTextColor(WHITE);
        d.setCursor(10, y);
        if (i == 0) { d.print("Style: "); d.print(screensaverStyleName()); }
        if (i == 1) { d.print("Idle: "); d.print(screensaverDelayName()); }
        if (i == 2) d.print("Preview now");
    }
    d.setTextColor(GREEN);
    d.setCursor(8, 104);
    d.print(saverMessage);
    d.setTextColor(GREY);
    d.setCursor(8, 116);
    d.print(", / or ENTER:Change / Preview");
    d.setCursor(8, 126);
    d.print("BACK:Settings  Idle:launcher only");
}

void updateSaverSettings() {
    if (showingSaverPreview) {
        screensaverUpdate();
        if (screensaverWantsExit()) {
            screensaverStop();
            showingSaverPreview = false;
            drawSaverSettings();
        }
        return; // Consume the wake key so it cannot also select a menu item.
    }
    auto& keyboard = M5Cardputer.Keyboard;
    if (!keyboard.isChange() || !keyboard.isPressed()) return;
    if (keyboard.isKeyPressed(KEY_BACKSPACE)) {
        showingSaverSettings = false;
        drawSettings();
        return;
    }
    saverMessage = "";
    if (keyboard.isKeyPressed(';')) saverSelected = (saverSelected + 2) % 3;
    else if (keyboard.isKeyPressed('.')) saverSelected = (saverSelected + 1) % 3;
    else if (keyboard.keysState().enter || keyboard.isKeyPressed(',') || keyboard.isKeyPressed('/')) {
        if (saverSelected == 2) {
            if (keyboard.keysState().enter) {
                showingSaverPreview = true;
                screensaverStart();
                return;
            }
        } else {
            int direction = keyboard.isKeyPressed(',') ? -1 : 1;
            bool saved = saverSelected == 0 ? screensaverChangeStyle(direction) : screensaverChangeDelay(direction);
            saverMessage = saved ? "Saved" : "Save failed; try again";
        }
    }
    drawSaverSettings();
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
    d.println("Baznetic OS v0.6");

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
bool showingWiFi = false;

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
    showingWiFi = false;
    showingSaverSettings = false;
    showingSaverPreview = false;
    selectedItem = 0;

    drawSettings();
}

void settingsUpdate() {

    if (showingSaverSettings) {
        updateSaverSettings();
        return;
    }

    if (showingWiFi) {
        wifiManagerUpdate();
        if (wifiManagerWantsExit()) {
            wifiManagerStop();
            showingWiFi = false;
            drawSettings();
        }
        return;
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
    else if (selectedItem == 1 &&
             keyboard.isKeyPressed(',')) {

        brightness = max(
            MIN_BRIGHTNESS,
            brightness - BRIGHTNESS_STEP
        );

        applyBrightness();
        saveBrightness();
        drawSettings();
    }

    else if (selectedItem == 1 &&
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

        if (selectedItem == 0) {
            showingWiFi = true;
            wifiManagerStart();
        } else if (selectedItem == 2) {
            showingSaverSettings = true;
            saverSelected = 0;
            saverMessage = "";
            drawSaverSettings();
        } else if (selectedItem == 3) {
            showingAbout = true;
            drawAbout();
        }
    }
}

void settingsStop() {
    if (showingSaverPreview) screensaverStop();
    showingSaverPreview = false;
    showingSaverSettings = false;
    if (showingWiFi) wifiManagerStop();
    showingWiFi = false;
    exitRequested = false;
}

bool settingsWantsExit() {
    return exitRequested;
}

#include <Arduino.h>
#include <Preferences.h>
#include "screensaver.h"
#include "matrix_screensaver.h"
#include "netware_screensaver.h"

namespace {
int style = 0;
int delayChoice = 0;
int runningStyle = 0;
const unsigned long DELAYS[] = {0, 30000, 60000, 120000, 300000};
const char* DELAY_NAMES[] = {"Manual only", "30 seconds", "1 minute", "2 minutes", "5 minutes"};

bool saveChoice(const char* key, int value) {
    Preferences prefs;
    if (!prefs.begin("baznetic", false)) return false;
    bool ok = prefs.putInt(key, value) == sizeof(int32_t);
    prefs.end();
    return ok;
}
} // namespace

void screensaverInit() {
    Preferences prefs;
    if (prefs.begin("baznetic", true)) {
        style = prefs.getInt("saver_style", 0);
        delayChoice = prefs.getInt("saver_delay", 0);
        prefs.end();
    }
    if (style < 0 || style > 1) style = 0;
    if (delayChoice < 0 || delayChoice > 4) delayChoice = 0;
}
const char* screensaverStyleName() { return style == 0 ? "Matrix" : "NetWare snakes"; }
int screensaverStyle() { return style; }
int screensaverDelayChoice() { return delayChoice; }
const char* screensaverDelayName() { return DELAY_NAMES[delayChoice]; }
unsigned long screensaverIdleTimeoutMs() { return DELAYS[delayChoice]; }

bool screensaverChangeStyle(int direction) {
    (void)direction; // Either direction toggles between the two styles.
    int next = (style + 1) % 2;
    if (!saveChoice("saver_style", next)) return false;
    style = next;
    return true;
}
bool screensaverChangeDelay(int direction) {
    int next = (delayChoice + (direction < 0 ? 4 : 1)) % 5;
    if (!saveChoice("saver_delay", next)) return false;
    delayChoice = next;
    return true;
}
void screensaverStart() {
    runningStyle = style;
    if (runningStyle == 0) matrixScreensaverStart();
    else netwareScreensaverStart();
}
void screensaverUpdate() {
    if (runningStyle == 0) matrixScreensaverUpdate();
    else netwareScreensaverUpdate();
}
void screensaverStop() {
    if (runningStyle == 0) matrixScreensaverStop();
    else netwareScreensaverStop();
}
bool screensaverWantsExit() {
    return runningStyle == 0 ? matrixScreensaverWantsExit() : netwareScreensaverWantsExit();
}

#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <atomic>
#include "wifi_manager.h"

namespace {
constexpr uint16_t BG = 0x0000, CYAN = 0x07FF, WHITE = 0xFFFF;
constexpr uint16_t GREEN = 0x07E0, GREY = 0x8410, SELECT = 0x3186;
constexpr unsigned long CONNECT_TIMEOUT = 20000, RETRY_INTERVAL = 30000;
enum Screen { MENU, NETWORKS, PASSWORD, DETAILS, FORGET };
struct Network { String ssid; int rssi; int channel; wifi_auth_mode_t auth; };
Screen screen = MENU;
std::vector<Network> networks;
String savedSSID, savedPassword, candidateSSID, candidatePassword, input;
String message;
int selected = 0, networkSelected = 0;
bool active = false, exitRequested = false, scanning = false;
std::atomic<bool> newIP{false};
bool connecting = false, saveOnSuccess = false, reconnectEnabled = false;
unsigned long attemptStarted = 0, lastAttempt = 0, scanStarted = 0, lastDraw = 0;

void clearSecret(String& value) {
    for (unsigned int i = 0; i < value.length(); ++i) value.setCharAt(i, 0);
    value = "";
}

// One versioned blob keeps SSID/password paired even if power is lost during save.
bool storeCredentials() {
    uint8_t data[99] = {};
    data[0] = 1;
    data[1] = candidateSSID.length();
    data[2] = candidatePassword.length();
    memcpy(data + 3, candidateSSID.c_str(), data[1]);
    memcpy(data + 35, candidatePassword.c_str(), data[2]);
    Preferences prefs;
    bool ok = prefs.begin("baznetic-wifi", false);
    if (ok) {
        ok = prefs.putBytes("credentials", data, sizeof(data)) == sizeof(data);
        prefs.end();
    }
    memset(data, 0, sizeof(data));
    return ok;
}

void stopScan() {
    if (scanning) esp_wifi_scan_stop();
    scanning = false;
    WiFi.scanDelete();
}

void beginConnection(const String& ssid, const String& password, bool save) {
    stopScan();
    newIP.store(false);
    WiFi.setAutoReconnect(false); // Retry policy lives in the main loop.
    WiFi.disconnect(false, true); // Clear driver credentials, keep STA enabled.
    candidateSSID = ssid;
    candidatePassword = password;
    saveOnSuccess = save;
    reconnectEnabled = true;
    connecting = true;
    attemptStarted = lastAttempt = millis();
    message = "Connecting...";
    WiFi.setMinSecurity(candidatePassword.isEmpty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_PSK);
    WiFi.begin(candidateSSID.c_str(), candidatePassword.c_str());
}

void disconnectNetwork() {
    reconnectEnabled = false;
    connecting = false;
    saveOnSuccess = false;
    clearSecret(candidatePassword);
    candidateSSID = "";
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, true);
    message = "Disconnected (saved retained)";
}

void header(const char* title) {
    auto& d = M5Cardputer.Display;
    d.fillScreen(BG);
    d.setTextWrap(false);
    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.print(title);
    d.drawLine(0, 28, 240, 28, CYAN);
    d.setTextSize(1);
    d.setTextColor(WHITE);
}

void line(int y, const String& text, uint16_t color = WHITE) {
    auto& d = M5Cardputer.Display;
    d.setTextColor(color);
    d.setCursor(8, y);
    d.print(text.substring(0, 37));
}

String connectionStatus() {
    if (connecting) return "Connecting...";
    if (WiFi.status() == WL_CONNECTED) return "Connected";
    if (reconnectEnabled && !savedSSID.isEmpty()) return "Offline; retry enabled";
    return "Disconnected";
}

void draw() {
    lastDraw = millis();
    auto& d = M5Cardputer.Display;
    if (screen == MENU) {
        header("WI-FI MANAGER");
        line(33, connectionStatus(), GREEN);
        const char* items[] = {"Scan / connect", "Connection details", "Reconnect saved", "Disconnect", "Forget saved network"};
        for (int i = 0; i < 5; ++i) {
            int y = 47 + i * 13;
            if (i == selected) d.fillRoundRect(4, y - 2, 232, 12, 3, SELECT);
            line(y, String(i == selected ? "> " : "  ") + items[i]);
        }
        line(114, message, GREEN);
        line(126, "ENTER:Select BACK:Settings", GREY);
    } else if (screen == NETWORKS) {
        header("SELECT NETWORK");
        line(33, scanning ? "Scanning..." : String(networks.size()) + " APs  R:Rescan");
        if (networks.empty() && !scanning) line(52, "No networks. R to retry");
        int first = networkSelected / 3 * 3;
        for (int row = 0; row < 3 && first + row < int(networks.size()); ++row) {
            int i = first + row, y = 48 + row * 23;
            const auto& n = networks[i];
            if (i == networkSelected) d.fillRoundRect(4, y - 2, 232, 22, 3, SELECT);
            line(y, n.ssid.isEmpty() ? "<Hidden SSID>" : n.ssid.substring(0, 32));
            line(y + 11, "CH:" + String(n.channel) + " " + String(n.rssi) + " dBm " + (n.auth == WIFI_AUTH_OPEN ? "OPEN" : "SECURED"), GREY);
        }
        line(114, message, GREEN);
        line(126, "ENTER:Select BACK:Menu", GREY);
    } else if (screen == PASSWORD) {
        header("WI-FI PASSWORD");
        line(35, candidateSSID);
        line(52, "Type password (8-63 characters)");
        String mask;
        for (unsigned int i = 0; i < input.length() && i < 30; ++i) mask += '*';
        line(69, mask, GREEN);
        line(83, String(input.length()) + " characters");
        line(99, message, GREEN);
        line(114, "ENTER:Connect BACK:Delete", GREY);
        line(126, "Fn+BACK:Cancel", GREY);
    } else if (screen == DETAILS) {
        header("WI-FI STATUS");
        line(33, connectionStatus(), GREEN);
        if (WiFi.status() == WL_CONNECTED) {
            line(46, "SSID: " + WiFi.SSID());
            line(59, "IP: " + WiFi.localIP().toString());
            line(72, "Gateway: " + WiFi.gatewayIP().toString());
            line(85, "RSSI: " + String(WiFi.RSSI()) + " dBm  CH:" + String(WiFi.channel()));
        } else {
            line(49, "Saved: " + (savedSSID.isEmpty() ? String("None") : savedSSID));
            line(66, message);
        }
        line(103, "Saved: " + (savedSSID.isEmpty() ? String("None") : savedSSID));
        line(126, "BACK:Menu", GREY);
    } else {
        header("FORGET NETWORK");
        line(42, savedSSID.isEmpty() ? "No saved network" : savedSSID);
        line(63, "Remove saved credentials?");
        line(83, "This also disconnects Wi-Fi.");
        line(114, message, GREEN);
        line(126, "ENTER:Forget BACK:Cancel", GREY);
    }
}

void startScan() {
    if (connecting) { message = "Wait for connection, then R"; draw(); return; }
    stopScan();
    networks.clear();
    networkSelected = 0;
    message = "";
    scanning = true;
    scanStarted = millis();
    if (WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED) {
        scanning = false;
        message = "Scan failed; R to retry";
    }
    draw();
}

void finishScan() {
    if (!scanning) return;
    int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING && millis() - scanStarted < 15000) return;
    if (count < 0) {
        stopScan();
        message = "Scan failed; R to retry";
    } else {
        scanning = false;
        // Limit indices to the Arduino WiFi API's uint8_t range.
        for (int i = 0; i < count && i < 256; ++i)
            networks.push_back({WiFi.SSID(i), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i)});
        WiFi.scanDelete();
    }
    draw();
}

bool personalSecurity(wifi_auth_mode_t auth) {
    return auth == WIFI_AUTH_WPA_PSK || auth == WIFI_AUTH_WPA2_PSK ||
           auth == WIFI_AUTH_WPA_WPA2_PSK || auth == WIFI_AUTH_WPA3_PSK ||
           auth == WIFI_AUTH_WPA2_WPA3_PSK;
}
} // namespace

void wifiManagerInit() {
    Preferences prefs;
    uint8_t data[99] = {};
    if (prefs.begin("baznetic-wifi", true)) {
        if (prefs.getBytesLength("credentials") == sizeof(data) &&
            prefs.getBytes("credentials", data, sizeof(data)) == sizeof(data) &&
            data[0] == 1 && data[1] > 0 && data[1] <= 32 && data[2] <= 63) {
            savedSSID = String(reinterpret_cast<char*>(data + 3), data[1]);
            savedPassword = String(reinterpret_cast<char*>(data + 35), data[2]);
        }
        prefs.end();
    }
    memset(data, 0, sizeof(data));
    WiFi.onEvent([](WiFiEvent_t) { newIP.store(true); }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    if (!savedSSID.isEmpty()) beginConnection(savedSSID, savedPassword, false);
}

void wifiManagerTick(bool allowRetry) {
    unsigned long now = millis();
    if (connecting) {
        if (newIP.load() && WiFi.status() == WL_CONNECTED && WiFi.SSID() == candidateSSID) {
            connecting = false;
            if (saveOnSuccess) {
                if (storeCredentials()) {
                    savedSSID = candidateSSID;
                    clearSecret(savedPassword);
                    savedPassword = candidatePassword;
                    message = "Connected; credentials saved";
                } else {
                    message = "Connected; save failed";
                    // Never retry an older network after a failed save.
                    reconnectEnabled = false;
                }
            } else message = "Connected";
            saveOnSuccess = false;
            clearSecret(candidatePassword);
            candidateSSID = "";
        } else if (now - attemptStarted >= CONNECT_TIMEOUT) {
            connecting = false;
            bool newNetwork = saveOnSuccess;
            saveOnSuccess = false;
            WiFi.disconnect(false, true);
            clearSecret(candidatePassword);
            candidateSSID = "";
            message = "Connection failed; try again";
            // Failed new credentials leave the previous saved profile intact.
            if (newNetwork) reconnectEnabled = false;
        }
    } else if (allowRetry && reconnectEnabled && !savedSSID.isEmpty() &&
               WiFi.status() != WL_CONNECTED && !scanning &&
               (!active || screen == MENU || screen == DETAILS) &&
               now - lastAttempt >= RETRY_INTERVAL &&
               WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
        beginConnection(savedSSID, savedPassword, false);
    }
}

bool wifiManagerIsConnecting() { return connecting; }
void wifiManagerStart() {
    active = true;
    exitRequested = false;
    screen = MENU;
    selected = 0;
    draw();
}
void wifiManagerUpdate() {
    finishScan();
    if ((screen == MENU || screen == DETAILS) && millis() - lastDraw >= 1000) draw();
    auto& keyboard = M5Cardputer.Keyboard;
    if (!keyboard.isChange() || !keyboard.isPressed()) return;
    auto keys = keyboard.keysState();
    bool back = keyboard.isKeyPressed(KEY_BACKSPACE);
    if (screen == PASSWORD) {
        if (keys.fn && back) {
            clearSecret(input);
            clearSecret(candidatePassword);
            candidateSSID = "";
            screen = NETWORKS;
            message = "";
        } else if (back) {
            if (!input.isEmpty()) input.remove(input.length() - 1);
            message = "";
        } else if (keys.enter) {
            if (input.length() < 8) message = "Password needs 8-63 characters";
            else {
                // Copy before beginConnection updates the candidate buffers.
                String ssid = candidateSSID;
                beginConnection(ssid, input, true);
                clearSecret(input);
                screen = DETAILS;
            }
        } else {
            for (char c : keys.word) {
                if (c >= 32 && c <= 126 && input.length() < 63) input += c;
            }
            message = "";
        }
        draw();
        return;
    }
    if (back) {
        if (screen == MENU) exitRequested = true;
        else { if (screen == NETWORKS) stopScan(); screen = MENU; message = ""; draw(); }
        return;
    }
    if (screen == MENU) {
        if (keyboard.isKeyPressed(';')) selected = (selected + 4) % 5;
        else if (keyboard.isKeyPressed('.')) selected = (selected + 1) % 5;
        else if (keys.enter) {
            if (selected == 0) { screen = NETWORKS; startScan(); return; }
            if (selected == 1) screen = DETAILS;
            if (selected == 2) {
                if (savedSSID.isEmpty()) message = "No saved network";
                else { beginConnection(savedSSID, savedPassword, false); screen = DETAILS; }
            }
            if (selected == 3) disconnectNetwork();
            if (selected == 4) { screen = FORGET; message = ""; }
        }
    } else if (screen == NETWORKS) {
        if (keyboard.isKeyPressed('r') || keyboard.isKeyPressed('R')) { startScan(); return; }
        if (scanning || networks.empty()) return;
        if (keyboard.isKeyPressed(';')) networkSelected = (networkSelected + networks.size() - 1) % networks.size();
        else if (keyboard.isKeyPressed('.')) networkSelected = (networkSelected + 1) % networks.size();
        else if (keys.enter) {
            const auto& n = networks[networkSelected];
            if (connecting) message = "Wait for connection";
            else if (n.ssid.isEmpty()) message = "Hidden SSID not supported";
            else if (n.auth == WIFI_AUTH_OPEN) { beginConnection(n.ssid, "", true); screen = DETAILS; }
            else if (!personalSecurity(n.auth)) message = "Unsupported security type";
            else { candidateSSID = n.ssid; clearSecret(input); message = ""; screen = PASSWORD; }
        }
    } else if (screen == FORGET && keys.enter) {
        Preferences prefs;
        bool ok = prefs.begin("baznetic-wifi", false);
        if (ok) {
            ok = !prefs.isKey("credentials") || prefs.remove("credentials");
            prefs.end();
        }
        if (ok) {
            disconnectNetwork();
            savedSSID = "";
            clearSecret(savedPassword);
            clearSecret(input);
            message = "Saved credentials removed";
            screen = MENU;
        } else message = "Forget failed; try again";
    }
    draw();
}
void wifiManagerStop() {
    stopScan();
    networks.clear();
    clearSecret(input);
    active = false;
    exitRequested = false;
    M5Cardputer.Display.setTextWrap(true);
}
bool wifiManagerWantsExit() { return exitRequested; }

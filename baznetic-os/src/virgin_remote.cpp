#include <Arduino.h>
#include <M5Cardputer.h>
#include "ui_fonts.h"
#include <driver/rmt.h>
#include "virgin_remote.h"
#include "virgin_remote_codes.h"
#include "ir_waveform.h"

namespace {
constexpr gpio_num_t IR_PIN = GPIO_NUM_44;
constexpr rmt_channel_t CHANNEL = RMT_CHANNEL_0;
constexpr uint16_t BG = 0, CYAN = 0x07FF, WHITE = 0xFFFF;
constexpr uint16_t GREEN = 0x07E0, GREY = 0x8410, SELECT = 0x3186;
rmt_item32_t waveform[48]; // Remains alive until async transmission finishes.
bool ready = false, sending = false, exitRequested = false, keyboardReleased = false;
bool listMode = false;
int selected = 0;
uint32_t clockHz = 0;
unsigned long sentAt = 0;
String message;
String pendingName;

void line(int y, const String& text, uint16_t color = WHITE) {
    auto& d = M5Cardputer.Display;
    d.setCursor(8, y);
    d.setTextColor(color);
    d.print(text.substring(0, 37));
}
void draw() {
    if (uiLarge()) {
        auto& d=M5Cardputer.Display; uiHeading(d,"VIRGIN REMOTE");
        if (listMode) {
            int first=selected/4*4;
            for(int row=0;row<4 && first+row<int(VIRGIN_COMMAND_COUNT);++row) {
                int i=first+row,y=32+row*20;
                if(i==selected)d.fillRoundRect(4,y-2,232,19,3,SELECT);
                String name=VIRGIN_COMMANDS[i].name; name.replace('_',' '); uiLine(d,y,name);
            }
        } else {
            uiLine(d,32,"; . , / : Direction");
            uiLine(d,52,"ENTER:OK  0-9:Picks");
            uiLine(d,72,"P:Power U/D:Channel");
            uiLine(d,92,"G:Guide TAB:Buttons");
        }
        uiLine(d,116,message.startsWith("Sent:") || message.startsWith("Sending:")?message:String("TAB / BACK:Exit"),GREY); return;
    }
    M5Cardputer.Display.setTextFont(1);

    auto& d = M5Cardputer.Display;
    d.fillScreen(BG);
    d.setTextWrap(false);
    d.setTextSize(2);
    d.setTextColor(CYAN);
    d.setCursor(8, 5);
    d.print("VIRGIN REMOTE");
    d.drawLine(0, 28, 240, 28, CYAN);
    d.setTextSize(1);
    line(33, "Virgin360 / Arris DCX960 / IR", GREY);
    if (listMode) {
        int first = selected / 4 * 4;
        for (int row = 0; row < 4 && first + row < int(VIRGIN_COMMAND_COUNT); ++row) {
            int i = first + row, y = 48 + row * 15;
            if (i == selected) d.fillRoundRect(4, y - 2, 232, 14, 3, SELECT);
            line(y, VIRGIN_COMMANDS[i].name);
        }
    } else {
        line(48, "; Up   . Down   , Left   / Right");
        line(61, "ENTER:OK  0-9:Numbers  P:Power");
        line(74, "U:CH+ D:CH-  G:Guide  H:Menu");
        line(87, "I:Info R:Recordings O:OnDemand");
        line(100, "V:Play B:Pause N:Stop T:Record");
    }
    line(114, message, GREEN);
    line(126, listMode ? "ENTER:Send TAB:Keys BACK:Exit" : "TAB:All 32 buttons  BACK:Exit", GREY);
}

int findCommand(const char* name) {
    for (size_t i = 0; i < VIRGIN_COMMAND_COUNT; ++i)
        if (strcmp(VIRGIN_COMMANDS[i].name, name) == 0) return i;
    return -1;
}
void sendCommand(int index) {
    if (index < 0 || index >= int(VIRGIN_COMMAND_COUNT) || sending) return;
    if (!ready) { message = "IR unavailable; reopen app"; draw(); return; }
    IrPulse pulses[96];
    size_t count = 0;
    if (!encodeIR(VIRGIN_COMMANDS[index].durations, VIRGIN_DURATION_COUNT, clockHz, pulses, 96, count)) {
        message = "Invalid IR waveform"; draw(); return;
    }
    memset(waveform, 0, sizeof(waveform));
    for (size_t i = 0; i < count; ++i) {
        auto& item = waveform[i / 2];
        if (i % 2 == 0) { item.duration0 = pulses[i].ticks; item.level0 = pulses[i].mark; }
        else { item.duration1 = pulses[i].ticks; item.level1 = pulses[i].mark; }
    }
    // The stored 92,302 us final space is preserved; no extra repeat frames.
    esp_err_t err = rmt_write_items(CHANNEL, waveform, (count + 1) / 2, false);
    if (err == ESP_OK) {
        sending = true;
        sentAt = millis();
        pendingName = VIRGIN_COMMANDS[index].name;
        message = "Sending: " + pendingName;
    } else message = "IR send failed";
    draw();
}
void sendName(const char* name) { sendCommand(findCommand(name)); }
} // namespace

void virginRemoteStart() {
    exitRequested = false;
    listMode = false;
    selected = 0;
    keyboardReleased = !M5Cardputer.Keyboard.isPressed();
    sending = false;
    ready = false;
    rmt_config_t cfg = {};
    cfg.rmt_mode = RMT_MODE_TX;
    cfg.channel = CHANNEL;
    cfg.gpio_num = IR_PIN;
    cfg.clk_div = 80;
    cfg.mem_block_num = 1;
    cfg.tx_config.loop_en = false;
    cfg.tx_config.carrier_en = true;
    cfg.tx_config.carrier_freq_hz = VIRGIN_FREQUENCY;
    cfg.tx_config.carrier_duty_percent = VIRGIN_DUTY_PERCENT;
    cfg.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    cfg.tx_config.idle_output_en = true;
    cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    if (rmt_config(&cfg) == ESP_OK && rmt_driver_install(CHANNEL, 0, 0) == ESP_OK) {
        if (rmt_get_counter_clock(CHANNEL, &clockHz) == ESP_OK && clockHz > 0) ready = true;
        else rmt_driver_uninstall(CHANNEL);
    }
    message = ready ? "Point IR emitter at your TV box" : "IR initialization failed";
    draw();
}
void virginRemoteUpdate() {
    if (sending) {
        esp_err_t result = rmt_wait_tx_done(CHANNEL, 0);
        if (result == ESP_OK) {
            sending = false;
            message = "Sent: " + pendingName;
            draw();
        } else if (result != ESP_ERR_TIMEOUT || millis() - sentAt > 500) {
            rmt_tx_stop(CHANNEL);
            sending = false;
            message = "IR transmission failed";
            draw();
        }
    }
auto& keyboard = M5Cardputer.Keyboard;

// Only process a new keypress.
// Do not depend on the keyboardReleased flag.
if (!keyboard.isChange() || !keyboard.isPressed()) {
    return;
}

auto keys = keyboard.keysState();
    if (keyboard.isKeyPressed(KEY_BACKSPACE) || keyboard.isKeyPressed('q') || keyboard.isKeyPressed('Q')) {
        exitRequested = true;
        return;
    }
    if (sending) return;
    if (keys.tab) { listMode = !listMode; draw(); return; }
    if (listMode) {
        if (keyboard.isKeyPressed(';')) selected = (selected + VIRGIN_COMMAND_COUNT - 1) % VIRGIN_COMMAND_COUNT;
        else if (keyboard.isKeyPressed('.')) selected = (selected + 1) % VIRGIN_COMMAND_COUNT;
        else if (keys.enter) { sendCommand(selected); return; }
        draw();
        return;
    }
    if (keys.enter) sendName("OK");
    else if (keyboard.isKeyPressed(';')) sendName("Up");
    else if (keyboard.isKeyPressed('.')) sendName("Down");
    else if (keyboard.isKeyPressed(',')) sendName("Left");
    else if (keyboard.isKeyPressed('/')) sendName("Right");
    else for (char c : keys.word) {
        if (c >= '0' && c <= '9') { char name[] = {c, 0}; sendName(name); break; }
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        const char* name = nullptr;
        switch (c) {
            case 'p': name = "Power"; break;
            case 'u': case '+': name = "Channel_Up"; break;
            case 'd': case '-': name = "Channel_Down"; break;
            case 'g': name = "Guide"; break;
            case 'h': name = "Menu"; break;
            case 'i': name = "Info"; break;
            case 'r': name = "Recordings"; break;
            case 'o': name = "OnDemand"; break;
            case 'v': name = "Play"; break;
            case 'b': name = "Pause"; break;
            case 'n': name = "Stop"; break;
            case 't': name = "Record"; break;
        }
        if (name) { sendName(name); break; }
    }
}
void virginRemoteStop() {
    if (ready) {
        if (sending) rmt_tx_stop(CHANNEL);
        rmt_driver_uninstall(CHANNEL);
        pinMode(IR_PIN, OUTPUT);
        digitalWrite(IR_PIN, LOW);
    }
    ready = false;
    sending = false;
    exitRequested = false;
    M5Cardputer.Display.setTextWrap(true);
}
bool virginRemoteWantsExit() { return exitRequested; }

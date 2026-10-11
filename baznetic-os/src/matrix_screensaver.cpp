#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_system.h>
#include "matrix_screensaver.h"

namespace {
constexpr int WIDTH = 240, HEIGHT = 135;
constexpr int CELL_WIDTH = 8, CELL_HEIGHT = 9;
constexpr int COLUMNS = WIDTH / CELL_WIDTH, ROWS = HEIGHT / CELL_HEIGHT;
constexpr unsigned long FRAME_INTERVAL = 80;
constexpr char GLYPHS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$%&*+=<>";

struct Stream {
    int head;
    int length;
    int pace;
    int tick;
    char glyphs[ROWS];
};
Stream streams[COLUMNS];
M5Canvas canvas(&M5Cardputer.Display);
bool buffered = false;
bool exitRequested = false;
bool keyboardReleased = false;
unsigned long lastFrame = 0;

int pick(int limit) { return esp_random() % limit; }
char glyph() { return GLYPHS[pick(sizeof(GLYPHS) - 1)]; }

void reset(Stream& stream, bool initial) {
    stream.head = -pick(initial ? ROWS + 1 : 9) - 1;
    stream.length = 4 + pick(8);
    stream.pace = 1 + pick(3);
    stream.tick = pick(stream.pace);
    for (char& c : stream.glyphs) c = glyph();
}

void advance() {
    for (auto& stream : streams) {
        if (++stream.tick < stream.pace) continue;
        stream.tick = 0;
        ++stream.head;
        if (stream.head >= ROWS + stream.length) reset(stream, false);
        if (stream.head >= 0 && stream.head < ROWS)
            stream.glyphs[stream.head] = glyph();
    }
}

template<class Display>
void drawFrame(Display& d) {
    d.fillScreen(0x0000);
    d.setTextWrap(false);
    d.setTextFont(1);
    d.setTextSize(1);
    for (int col = 0; col < COLUMNS; ++col) {
        const auto& stream = streams[col];
        for (int tail = stream.length - 1; tail >= 0; --tail) {
            int row = stream.head - tail;
            if (row < 0 || row >= ROWS) continue;
            // RGB565: pale head, progressively dimmer green trail.
            uint16_t color = tail == 0 ? 0xBFF7 :
                uint16_t((5 + (stream.length - tail) * 24 / stream.length) << 5);
            d.setTextColor(color);
            d.setCursor(col * CELL_WIDTH + 1, row * CELL_HEIGHT);
            d.print(stream.glyphs[row]);
        }
    }

    // A dark window keeps the title readable amid the rain.
    d.fillRoundRect(42, 48, 156, 37, 6, 0x0000);
    d.drawRoundRect(40, 46, 160, 41, 7, 0x0200);
    d.drawRoundRect(42, 48, 156, 37, 6, 0x03E0);
    d.setTextSize(2);
    int x = (WIDTH - d.textWidth("BAZNETIC OS")) / 2;
    d.setTextColor(0x03E0);
    d.setCursor(x + 1, 59);
    d.print("BAZNETIC OS");
    d.setTextColor(0xBFF7);
    d.setCursor(x, 58);
    d.print("BAZNETIC OS");

    d.setTextSize(1);
    d.fillRect(39, 122, 162, 11, 0x0000);
    d.setTextColor(0x07E0);
    d.setCursor((WIDTH - d.textWidth("ANY KEY: Back")) / 2, 124);
    d.print("ANY KEY: Back");
}

void render() {
    if (buffered) {
        drawFrame(canvas);
        canvas.pushSprite(0, 0);
    } else {
        // Still usable if the frame buffer cannot be allocated.
        drawFrame(M5Cardputer.Display);
    }
}
} // namespace

void matrixScreensaverStart() {
    exitRequested = false;
    // Require release of S before accepting a wake key.
    keyboardReleased = !M5Cardputer.Keyboard.isPressed();
    for (auto& stream : streams) reset(stream, true);
    canvas.setColorDepth(8); // 32 KB buffer for smooth full-frame updates.
    buffered = canvas.createSprite(WIDTH, HEIGHT) != nullptr;
    lastFrame = millis();
    render();
}

void matrixScreensaverUpdate() {
    if (!M5Cardputer.Keyboard.isPressed()) keyboardReleased = true;
    else if (keyboardReleased) {
        exitRequested = true;
        return;
    }

    unsigned long now = millis();
    if (now - lastFrame < FRAME_INTERVAL) return;
    lastFrame = now;
    advance();
    render();
}

void matrixScreensaverStop() {
    canvas.deleteSprite();
    buffered = false;
    exitRequested = false;
    M5Cardputer.Display.setTextWrap(true);
    M5Cardputer.Display.setTextFont(1);
    M5Cardputer.Display.setTextSize(1);
}

bool matrixScreensaverWantsExit() { return exitRequested; }

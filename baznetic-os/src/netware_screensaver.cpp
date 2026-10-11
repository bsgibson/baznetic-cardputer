#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_system.h>
#include "netware_screensaver.h"
#include "live_load.h"

namespace {
constexpr int WIDTH = 240, HEIGHT = 135, CELL = 4;
constexpr int COLS = WIDTH / CELL, ROWS = 24, TOP = 24;
constexpr int SNAKES = 2, MAX_LENGTH = 48;
constexpr unsigned long FRAME_MS = 60;
constexpr int DX[] = {1, 0, -1, 0}, DY[] = {0, 1, 0, -1};
struct Point { int x, y; };
struct Snake {
    Point body[MAX_LENGTH]; // Head first; tails follow the exact previous path.
    int count = 0;
    int length = 32;
    int direction = 0;
    unsigned long lastStep = 0;
    unsigned long stepInterval = 500;
    uint16_t color = 0x07E0;
};
Snake snakes[SNAKES];
M5Canvas canvas(&M5Cardputer.Display);
bool buffered = false, exitRequested = false, keyboardReleased = false;
unsigned long lastFrame = 0;
int pick(int limit) { return esp_random() % limit; }
bool same(Point a, Point b) { return a.x == b.x && a.y == b.y; }

bool occupied(Point p, int movingSnake) {
    for (int i = 0; i < SNAKES; ++i) {
        int count = snakes[i].count;
        // The moving snake's last cell is vacated during a full-length step.
        if (i == movingSnake && count == snakes[i].length) --count;
        for (int j = 0; j < count; ++j)
            if (same(p, snakes[i].body[j])) return true;
    }
    return false;
}

bool canMove(int index, int direction) {
    Point head = snakes[index].body[0];
    Point next = {head.x + DX[direction], head.y + DY[direction]};
    return next.x >= 0 && next.x < COLS && next.y >= 0 && next.y < ROWS &&
           !occupied(next, index);
}

void advanceSnake(int index) {
    auto& snake = snakes[index];
    unsigned long now = millis();
    if (now - snake.lastStep < snake.stepInterval) return;
    snake.lastStep = now;
    int direction = snake.direction;
    int left = (direction + 3) % 4, right = (direction + 1) % 4;
    // Mostly straight, with occasional right-angle turns like console snakes.
    if (pick(100) < 22) {
        int turn = pick(2) ? left : right;
        if (canMove(index, turn)) direction = turn;
    }
    if (!canMove(index, direction)) {
        int first = pick(2) ? left : right;
        int second = first == left ? right : left;
        if (canMove(index, first)) direction = first;
        else if (canMove(index, second)) direction = second;
        else {
            // Uncoil when boxed in; never index outside the display or body.
            snake.count = 1;
            direction = (snake.direction + 2) % 4;
            if (!canMove(index, direction)) return;
        }
    }
    Point next = {snake.body[0].x + DX[direction], snake.body[0].y + DY[direction]};
    int count = min(snake.count + 1, snake.length);
    for (int i = count - 1; i > 0; --i) snake.body[i] = snake.body[i - 1];
    snake.body[0] = next;
    snake.count = count;
    snake.direction = direction;
}

void applyLoad() {
    int heap = liveLoadHeap();
    for (int i = 0; i < SNAKES; ++i) {
        int cpu = liveLoadCPU(i);
        // Idle: 2 steps/sec; fully busy: up to one step per 60 ms frame.
        snakes[i].stepInterval = cpu < 0 ? 500 : 500 - cpu * 440 / 100;
        snakes[i].length = heap < 0 ? 12 : 12 + heap * (MAX_LENGTH - 12) / 100;
        snakes[i].count = min(snakes[i].count, snakes[i].length);
    }
}

String reading(int value) { return value < 0 ? String("--") : String(value) + "%"; }

template<class Display>
void drawFrame(Display& d) {
    d.fillScreen(0x0000);
    d.setTextWrap(false);
    d.setTextFont(1);
    d.setTextSize(1);
    d.setTextColor(0x8410);
    d.setCursor(4, 2);
    d.print("NETWARE / SAMPLED CPU LOAD");
    d.setCursor(4, 13);
    d.setTextColor(0x07E0);
    d.print("C0:" + reading(liveLoadCPU(0)));
    d.setCursor(80, 13);
    d.setTextColor(0xFFE0);
    d.print("C1:" + reading(liveLoadCPU(1)));
    d.setCursor(154, 13);
    d.setTextColor(0x8410);
    d.print("Heap:" + reading(liveLoadHeap()));
    for (int index = 0; index < SNAKES; ++index) {
        const auto& snake = snakes[index];
        for (int i = snake.count - 1; i >= 0; --i) {
            const auto& p = snake.body[i];
            d.fillRect(p.x * CELL, TOP + p.y * CELL, CELL - 1, CELL - 1,
                       i == 0 ? 0xFFFF : snake.color);
        }
    }
    d.setTextColor(0x8410);
    d.setCursor((WIDTH - d.textWidth("ANY KEY:Back  Length:heap")) / 2, 126);
    d.print("ANY KEY:Back  Length:heap");
}
void render() {
    if (buffered) { drawFrame(canvas); canvas.pushSprite(0, 0); }
    else drawFrame(M5Cardputer.Display);
}
} // namespace

void netwareScreensaverStart() {
    exitRequested = false;
    keyboardReleased = !M5Cardputer.Keyboard.isPressed();
    for (int i = 0; i < SNAKES; ++i) {
        snakes[i].body[0] = {COLS / 4 + i * COLS / 2, ROWS / 3 + i * ROWS / 3};
        snakes[i].count = 1;
        snakes[i].length = 12;
        snakes[i].direction = i == 0 ? 0 : 2;
        snakes[i].stepInterval = 500;
        snakes[i].lastStep = millis();
        snakes[i].color = i == 0 ? 0x07E0 : 0xFFE0;
    }
    canvas.setColorDepth(8);
    buffered = canvas.createSprite(WIDTH, HEIGHT) != nullptr;
    liveLoadStart();
    applyLoad();
    lastFrame = millis();
    render();
}
void netwareScreensaverUpdate() {
    if (!M5Cardputer.Keyboard.isPressed()) keyboardReleased = true;
    else if (keyboardReleased) { exitRequested = true; return; }
    liveLoadUpdate();
    applyLoad();
    unsigned long now = millis();
    if (now - lastFrame < FRAME_MS) return;
    lastFrame = now;
    for (int i = 0; i < SNAKES; ++i) advanceSnake(i);
    render();
}
void netwareScreensaverStop() {
    liveLoadStop();
    canvas.deleteSprite();
    buffered = false;
    exitRequested = false;
    M5Cardputer.Display.setTextWrap(true);
    M5Cardputer.Display.setTextFont(1);
    M5Cardputer.Display.setTextSize(1);
}
bool netwareScreensaverWantsExit() { return exitRequested; }

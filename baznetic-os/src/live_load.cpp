#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_freertos_hooks.h>
#include "live_load.h"

namespace {
constexpr int MAX_CORES = 2;
portMUX_TYPE sampleMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t samples[MAX_CORES] = {};
volatile uint32_t busy[MAX_CORES] = {};
TaskHandle_t idleTask[MAX_CORES] = {};
bool registered[MAX_CORES] = {};
int cpuPercent[MAX_CORES] = {-1, -1};
int heapPercent = -1;
unsigned long lastSample = 0;

// Tick hooks execute in interrupt context: no allocation, printing or blocking.
void IRAM_ATTR sampleCore(int core) {
    bool nonIdle = xTaskGetCurrentTaskHandle() != idleTask[core];
    portENTER_CRITICAL_ISR(&sampleMux);
    ++samples[core];
    if (nonIdle) ++busy[core];
    portEXIT_CRITICAL_ISR(&sampleMux);
}
void IRAM_ATTR sampleCore0() { sampleCore(0); }
void IRAM_ATTR sampleCore1() { sampleCore(1); }
const esp_freertos_tick_cb_t callbacks[MAX_CORES] = {sampleCore0, sampleCore1};

void readHeap() {
    uint32_t total = ESP.getHeapSize(), free = ESP.getFreeHeap();
    heapPercent = total == 0 ? -1 : int(uint64_t(total - min(total, free)) * 100 / total);
}
} // namespace

void liveLoadStart() {
    liveLoadStop(); // Safe on initial start and repeated previews.
    portENTER_CRITICAL(&sampleMux);
    for (int core = 0; core < MAX_CORES; ++core) {
        samples[core] = busy[core] = 0;
        cpuPercent[core] = -1;
    }
    portEXIT_CRITICAL(&sampleMux);
    for (int core = 0; core < MAX_CORES && core < portNUM_PROCESSORS; ++core) {
        idleTask[core] = xTaskGetIdleTaskHandleForCPU(core);
        if (idleTask[core] != nullptr)
            registered[core] = esp_register_freertos_tick_hook_for_cpu(callbacks[core], core) == ESP_OK;
    }
    lastSample = millis();
    readHeap();
}
void liveLoadUpdate() {
    unsigned long now = millis();
    if (now - lastSample < 1000) return;
    lastSample = now;
    uint32_t totals[MAX_CORES], working[MAX_CORES];
    portENTER_CRITICAL(&sampleMux);
    for (int core = 0; core < MAX_CORES; ++core) {
        totals[core] = samples[core];
        working[core] = busy[core];
        samples[core] = busy[core] = 0;
    }
    portEXIT_CRITICAL(&sampleMux);
    for (int core = 0; core < MAX_CORES; ++core)
        cpuPercent[core] = registered[core] && totals[core] > 0 ?
            int(uint64_t(min(working[core], totals[core])) * 100 / totals[core]) : -1;
    readHeap();
}
void liveLoadStop() {
    for (int core = 0; core < MAX_CORES; ++core) {
        if (registered[core]) esp_deregister_freertos_tick_hook_for_cpu(callbacks[core], core);
        registered[core] = false;
        cpuPercent[core] = -1;
    }
    heapPercent = -1;
}
int liveLoadCPU(int core) { return core >= 0 && core < MAX_CORES ? cpuPercent[core] : -1; }
int liveLoadHeap() { return heapPercent; }

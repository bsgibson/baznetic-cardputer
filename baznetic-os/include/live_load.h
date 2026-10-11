#pragma once

void liveLoadStart();
void liveLoadUpdate();
void liveLoadStop();
// -1 means unavailable or the first one-second sample is still pending.
int liveLoadCPU(int core);
// Percentage of the Arduino-reported heap currently allocated; -1 if unknown.
int liveLoadHeap();

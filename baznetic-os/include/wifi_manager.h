#pragma once

void wifiManagerInit();
// Called in every app; retries are deferred while a scanning app is active.
void wifiManagerTick(bool allowRetry);
bool wifiManagerIsConnecting();
void wifiManagerStart();
void wifiManagerUpdate();
void wifiManagerStop();
bool wifiManagerWantsExit();

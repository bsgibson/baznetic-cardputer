#pragma once

void wifiScannerStart();
void wifiScannerUpdate();
void wifiScannerStop();

// True when the scanner requests a return to the OS launcher.
bool wifiScannerWantsExit();
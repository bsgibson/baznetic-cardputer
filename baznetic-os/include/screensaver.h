#pragma once

void screensaverInit();
const char* screensaverStyleName();
int screensaverStyle();
int screensaverDelayChoice();
const char* screensaverDelayName();
unsigned long screensaverIdleTimeoutMs();
bool screensaverChangeStyle(int direction);
bool screensaverChangeDelay(int direction);
void screensaverStart();
void screensaverUpdate();
void screensaverStop();
bool screensaverWantsExit();

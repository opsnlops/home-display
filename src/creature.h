#pragma once

#define CREATURE_NAME "home-display"
#define CREATURE_POWER "mains"
#define CREATURE_TOPIC "creatures/home-display"


// Toss some things into the global namespace so that the libs can read it
const char* gCreatureName = CREATURE_NAME;

// Is the WiFi connected?
boolean gWifiConnected = false;

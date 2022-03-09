
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "logging/logging.h"

#include "creature.h"

void updateConfig(String incomingJson);

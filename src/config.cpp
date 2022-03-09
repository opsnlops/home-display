
#include <Arduino.h>
#include <ArduinoJson.h>

#include "logging/logging.h"

using namespace creatures;

extern boolean gDisplayOn;

static Logger l;

/**
 * @brief Update the configuration of the device from MQTT
 *
 * The config isn't persisted anywhere on the MCU. It's config it's kept in a retained MQTT topic
 * which will be read when it boots.
 *
 * @param incomingJson a String with JSON from MQTT
 */
void updateConfig(String incomingJson)
{
    l.debug("Incoming config message: %s", incomingJson.c_str());

    // Let's put this on the stack so it goes poof when we leave
    StaticJsonDocument<512> json;

    DeserializationError error = deserializeJson(json, incomingJson);

    if (error)
    {
        l.error("Unable to deserialize config from MQTT: %s", error.c_str());
    }
    else
    {
        l.debug("decode was good!");

        String display = json["display"];
        l.debug("'display' was %s", display.c_str());

        if (display.equals("on"))
        {
            gDisplayOn = true;
            l.info("Turned on the display");
        }
        else
        {
            gDisplayOn = false;
            l.info("Turned OFF the display");
        }
    }
}

#ifndef _APRILS_CREATURES_MQTT
#define _APRILS_CREATURES_MQTT

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

#include "logging/logging.h"

#define SL_CONNCURRENCY_TOPIC "external/secondlife/concurrency"

#define BATHROOM_MOTION_TOPIC "home/bathroom/motion"
#define BEDROOM_MOTION_TOPIC "home/bedroom/motion"
#define OFFICE_MOTION_TOPIC "home/office/motion"
#define LAUNDRY_ROOM_MOTION_TOPIC "home/laundry_room/motion"
#define LIVING_ROOM_MOTION_TOPIC "home/living_room/motion"
#define WORKSHOP_MOTION_TOPIC "home/workshop/motion"

#define BATHROOM_TEMPERATURE_TOPIC "home/bathroom/temperature"
#define BEDROOM_TEMPERATURE_TOPIC "home/bedroom/temperature"
#define OFFICE_TEMPERATURE_TOPIC "home/office/temperature"
#define LAUNDRY_ROOM_TEMPERATURE_TOPIC "home/laundry_room/temperature"
#define LIVING_ROOM_TEMPERATURE_TOPIC "home/living_room/temperature"
#define WORKSHOP_TEMPERATURE_TOPIC "home/workshop/temperature"

#define OUTSIDE_TEMPERATURE_TOPIC "home/outside/temperature"


#define MQTT_ON "on"
#define MQTT_OFF "off"

void connectToMqtt(IPAddress mqtt_broker_address, uint16_t mqtt_broker_port);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttSubscribe(uint16_t packetId, uint8_t qos);
void onMqttUnsubscribe(uint16_t packetId);
void onMqttPublish(uint16_t packetId);
void onWifiDisconnect();


#endif

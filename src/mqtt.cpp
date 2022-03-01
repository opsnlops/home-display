

#include <AsyncMqttClient.h>

#include "creature.h"
#include "mqtt.h"
#include "main.h"
#include "logging/logging.h"

static creatures::Logger l;

AsyncMqttClient mqttClient;

// These are defined in main.cpp
extern TimerHandle_t mqttReconnectTimer;
// extern TimerHandle_t wifiReconnectTimer;

void connectToMqtt(IPAddress mqtt_broker_address, uint16_t mqtt_broker_port)
{

  l.debug("Trying to connect to MQTT");
  mqttClient.setServer(mqtt_broker_address, mqtt_broker_port);

  l.info("Connecting to MQTT...");
  show_startup("Connecting to MQTT");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  l.info("Connected to MQTT.");
  l.verbose("Session present: %b", sessionPresent);
  show_startup("Connected to MQTT");

  // What topics do we care about?
  mqttClient.subscribe(SL_CONNCURRENCY_TOPIC, 0);

  mqttClient.subscribe(BATHROOM_MOTION_TOPIC, 2);
  mqttClient.subscribe(BEDROOM_MOTION_TOPIC, 2);
  mqttClient.subscribe(OFFICE_MOTION_TOPIC, 2);
  mqttClient.subscribe(LAUNDRY_ROOM_MOTION_TOPIC, 2);
  mqttClient.subscribe(LIVING_ROOM_MOTION_TOPIC, 2);
  mqttClient.subscribe(WORKSHOP_MOTION_TOPIC, 2);

  mqttClient.subscribe(BATHROOM_TEMPERATURE_TOPIC, 0);
  mqttClient.subscribe(BEDROOM_TEMPERATURE_TOPIC, 0);
  mqttClient.subscribe(OFFICE_TEMPERATURE_TOPIC, 0);
  mqttClient.subscribe(LAUNDRY_ROOM_TEMPERATURE_TOPIC, 0);
  mqttClient.subscribe(LIVING_ROOM_TEMPERATURE_TOPIC, 0);
  mqttClient.subscribe(WORKSHOP_TEMPERATURE_TOPIC, 0);

  mqttClient.subscribe(OUTSIDE_TEMPERATURE_TOPIC, 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  l.info("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  l.debug("Subscribe acknowledged.");
  l.verbose("  packetId: %d, qos: %d", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
  l.info("Unsubscribe acknowledged.");
  l.verbose("  packetId: %d", packetId);
}

void onMqttPublish(uint16_t packetId)
{
  l.info("Publish acknowledged.");
  l.verbose("  packetId: %d", packetId);
}

void onWifiDisconnect()
{
  // Reset the broker IP so we try to find it next time
  // mqtt_broker_address = DEFAULT_IP_ADDRESS;
}

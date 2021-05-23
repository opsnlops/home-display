

#include <AsyncMqttClient.h>

#include "creature.h"
#include "connection.h"
#include "mqtt.h"
#include "main.h"

const char *broker_role = "magic";
const char *broker_service = "mqtt";
const char *broker_protocol = "tcp";

IPAddress mqtt_broker_address = DEFAULT_IP_ADDRESS;
uint16_t mqtt_broker_port = DEFAULT_MQTT_PORT;

AsyncMqttClient mqttClient;

// These are defined in main.cpp
extern TimerHandle_t mqttReconnectTimer;
extern TimerHandle_t wifiReconnectTimer;

void connectToMqtt()
{

  // Make sure we have a broker
  while (mqtt_broker_address == DEFAULT_IP_ADDRESS)
  {
    mqtt_broker_address = find_broker(broker_service, broker_protocol);
    log_i("The IP of the broker is %s", mqtt_broker_address.toString().c_str());
  }

  mqttClient.setServer(mqtt_broker_address, mqtt_broker_port);

  log_i("Connecting to MQTT...");
  show_startup("Connecting to MQTT");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  log_i("Connected to MQTT.");
  log_v("Session present: %b", sessionPresent);
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
  mqttClient.subscribe(OUTSIDE_WIND_SPEED, 0);
  //mqttClient.subscribe(OUTSIDE_WIND_BEARING, 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  log_i("Disconnected from MQTT.");

  // Reset the broker to the default
  mqtt_broker_address = DEFAULT_IP_ADDRESS;

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  log_d("Subscribe acknowledged.");
  log_v("  packetId: %d, qos: %d", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
  log_i("Unsubscribe acknowledged.");
  log_v("  packetId: %d", packetId);
}

void onMqttPublish(uint16_t packetId)
{
  log_i("Publish acknowledged.");
  log_v("  packetId: %d", packetId);
}

void onWifiDisconnect()
{
  // Reset the broker IP so we try to find it next time
  mqtt_broker_address = DEFAULT_IP_ADDRESS;
}

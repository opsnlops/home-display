

#include <AsyncMqttClient.h>

#include "creature.h"
#include "connection.h"
#include "mqtt.h"

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
    Serial.println("The IP of the broker is " + mqtt_broker_address.toString());
  }

  mqttClient.setServer(mqtt_broker_address, mqtt_broker_port);

  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

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
  Serial.println("Disconnected from MQTT.");

  // Reset the broker to the default
  mqtt_broker_address = DEFAULT_IP_ADDRESS;

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onWifiDisconnect()
{
  // Reset the broker IP so we try to find it next time
  mqtt_broker_address = DEFAULT_IP_ADDRESS;
}

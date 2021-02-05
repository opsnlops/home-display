

#include <AsyncMqttClient.h>

#include "creature.hpp"
#include "connection.hpp"
#include "mqtt.hpp"

const char *broker_role = "magic";
const char *broker_service = "mqtt";
const char *broker_protocol = "tcp";

IPAddress mqtt_broker_address = DEFAULT_IP_ADDRESS;
uint16_t mqtt_broker_port = 1883;

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

  uint16_t packetIdSub = mqttClient.subscribe("#", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);

  mqttClient.publish(CREATURE_TOPIC, 0, true, "test 1");
  Serial.println("Publishing at QoS 0");
  uint16_t packetIdPub1 = mqttClient.publish(CREATURE_TOPIC, 1, true, "test 2");
  Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);
  uint16_t packetIdPub2 = mqttClient.publish(CREATURE_TOPIC, 2, true, "test 3");
  Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

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

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  digitalWrite(LED_BUILTIN, HIGH);

  char payload_string[len];
  memcpy(payload_string, payload, len);

  int size = sizeof(payload_string) / sizeof(payload_string[0]);
  Serial.print("\n\npayload length: ");
  Serial.println(size);

  //for (int i = 0; i < size; i++)
  //{
  //  Serial.println(payload_string[i]);
  // }

  Serial.println("Message received:");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  payload: ");
  Serial.println(payload_string);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);

  digitalWrite(LED_BUILTIN, LOW);
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}
// This isn't gonna work on anything but an ESP32
#if !(defined(ESP8266) || defined(ESP32))
#error This code is intended to run only on the ESP8266 and ESP32 boards
#endif

#include <WiFi.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

#include <SPI.h>
#include <SD.h>
#include <ESP32Servo.h>

#include <creatures.h>
#include "secrets.h"
#include "connection.h"

#define CREATURE_NAME "Sockey"
#define CREATURE_TOPIC "creatures/sockey"

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

IPAddress mqtt_broker_address = default_ip_address;
uint16_t mqtt_broker_port = 1883;

// WiFi network name and password:
const char *network_name = WIFI_NETWORK;
const char *network_password = WIFI_PASSWORD;

const char *broker_role = "magic";
const char *broker_service = "mqtt";
const char *broker_protocol = "tcp";

File myFile;

#define FILE_NAME "/two.aaw"

// We need to use ADC1. ADC2 is used by the Wifi. (Pins GPIO32-GPIO39)
Servo servos[2];
const int servo0Pin = 32;
const int servo1Pin = 33;

// RAWR!
uint8_t MAGIC_NUMBER_ARRAY[5] = {0x52, 0x41, 0x57, 0x52, 0x21};

// Keep a counter of the frames we've played
uint16_t currentFrame = 0;

void config_fail()
{
  while (true)
  {
    digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
    delay(100);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
    delay(100);                      // wait for a second
  }
}

bool check_file(File *file)
{

  // Our magic number is 5 bytes long
  const uint8_t magic_number_size = 5;

  bool return_code = false;

  // Make sure the first five bytes are our magic number
  char *buffer = (char *)malloc(sizeof(uint8_t) * magic_number_size);

  file->readBytes(buffer, magic_number_size);

  for (int i = 0; i < magic_number_size; i++)
  {
    if (buffer[i] != MAGIC_NUMBER_ARRAY[i])
    {
      Serial.print("Magic Number fail at position ");
      Serial.println(i);
      return_code = false;
    }
  }

  // If we made it this far, we're good
  return_code = true;

  free(buffer);

  return return_code;
}

// Returns the header from the file
struct Header read_header(File *file)
{
  struct Header header;

  file->readBytes((char *)&header, sizeof(Header));

  Serial.print("number of servos: ");
  Serial.print(header.number_of_servos);
  Serial.print(", number of frames: ");
  Serial.print(header.number_of_frames);
  Serial.print(", ms per frame: ");
  Serial.println(header.time_per_frame);

  return header;
}

void play_frame(File *file, size_t number_of_servos)
{
  uint8_t servo[number_of_servos];
  file->readBytes((char *)&servo, number_of_servos);
  for (int i = 0; i < number_of_servos; i++)
  {
    Serial.println(servo[i]);
    servos[i].write(servo[i]);
  }
}

void connectToMqtt()
{

  // Make sure we have a broker
  while (mqtt_broker_address == default_ip_address)
  {
    mqtt_broker_address = find_broker(broker_service, broker_protocol);
    Serial.println("The IP of the broker is " + mqtt_broker_address.toString());
  }

  mqttClient.setServer(mqtt_broker_address, mqtt_broker_port);

  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void connect_wifi()
{
  connectToWiFi();
}

void WiFiEvent(WiFiEvent_t event)
{
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    connectToMqtt();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    break;
  }
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
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
    ;
  delay(200);

  Serial.println("attaching to servo 0");
  servos[0].setPeriodHertz(50);
  servos[0].attach(servo0Pin);
  Serial.println("done");

  Serial.println("attaching to servo 1");
  servos[1].setPeriodHertz(50);
  servos[1].attach(servo1Pin);
  Serial.println("done");

  if (!MDNS.begin(CREATURE_NAME))
  {
    Serial.println("Error setting up mDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("MDNS set up");

  // We aren't _really_ running something on tcp/666, but this lets me
  // find the IP of the creature from an mDNS browser
  MDNS.addService("creature", "tcp", 666);
  Serial.println("added our fake mDNS service");

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connect_wifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  connect_wifi();

  /*
  // Set up the SD card


  Serial.print("Initializing SD card...");
  if (!SD.begin(5))
  {
    Serial.println("initialization failed!");
    while (1)
      ;
  }
  Serial.println("initialization done.");
  // open the file for reading:
  myFile = SD.open(FILE_NAME);
  if (myFile)
  {
    Serial.println(FILE_NAME);

    if (!check_file(&myFile))
    {
      config_fail();
    }

    struct Header header = read_header(&myFile);

    // read from the file until there's nothing else in it:
    while (myFile.available())
    {

      uint8_t command = myFile.read();
      if (command == (uint8_t)MOVEMENT_FRAME_TYPE)
      {
        play_frame(&myFile, header.number_of_servos);
        delay(header.time_per_frame);
      }

      if (currentFrame % 10 == 0)
      {
        Serial.print("frame ");
        Serial.println(currentFrame);
      }

      currentFrame++;
    }
    // close the file:
    myFile.close();
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(FILE_NAME);
    config_fail();
  }
  */
}

void loop()
{
  // nothing happens after setup
}

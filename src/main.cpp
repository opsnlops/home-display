// This isn't gonna work on anything but an ESP32
#if !(defined(ESP8266) || defined(ESP32))
#error This code is intended to run only on the ESP8266 and ESP32 boards
#endif

#include <Arduino.h>

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
#include <LiquidCrystal.h>
#include "time.h"

#include <creatures.h>
#include "creature.h"
#include "secrets.h"
#include "mqtt.h"
#include "connection.h"

extern AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;


const int rs = 12, en = 14, d4 = 26, d5 = 25, d6 = 27, d7 = 33;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);



void paint_lcd(String top_line, String bottom_line)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(top_line);
  lcd.setCursor(0, 1);
  lcd.print(bottom_line);
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
    paint_lcd("My IP", WiFi.localIP().toString());
    connectToMqtt();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    paint_lcd("Oh no!", "Wifi connection lost!");
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    onWifiDisconnect(); // Tell the broker we lost Wifi
    break;
  }
}

void set_up_lcd()
{
  const int rs = 12, en = 14, d4 = 26, d5 = 25, d6 = 27, d7 = 33;
  lcd = LiquidCrystal(rs, en, d4, d5, d6, d7);
  lcd.begin(16, 2);
  paint_lcd(CREATURE_NAME, "Starting up...");
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
    ;
  // Nice long delay to let minicom start
  delay(5000);

  set_up_lcd();


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
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWiFi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  connectToWiFi();
  digitalWrite(LED_BUILTIN, LOW);

  // TODO: Remove this, don't talk to the Internet
  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = -28800;
  const int   daylightOffset_sec = 3600;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

}

// Stolen from StackOverflow
char *ultoa(unsigned long val, char *s)
{
    char *p = s + 13;
    *p = '\0';
    do {
        if ((p - s) % 4 == 2)
            *--p = ',';
        *--p = '0' + val % 10;
        val /= 10;
    } while (val);
    return p;
}

void display_message(const char *topic, const char *message)
{
  int topic_length = strlen(topic);
  if(strncmp(SL_CONNCURRENCY_TOPIC, topic, topic_length) == 0)
  {
    unsigned long concurrency = atol(message);
    char buffer[14];
    Serial.println(ultoa(concurrency, buffer));
  }
  else if(strncmp(LIVING_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if(strncmp(MQTT_ON, message, 2) == 0)
    {
      Serial.println("Living Room Motion");
    }
    else
    {
      Serial.println("Living Room Cleared");
    }
  }
  else if(strncmp(WORKSHOP_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if(strncmp(MQTT_ON, message, 2) == 0)
    {
      Serial.println("Workshop Motion");
    }
    else
    {
      Serial.println("Workshop Cleared");
    }
  }
  else
  {
    Serial.print(topic);
    Serial.print(" ");
    Serial.println(message);
  }

  
}


void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  digitalWrite(LED_BUILTIN, HIGH);

  // MQTT allows binary content to be passed, so payload will arrive without a
  // NUL at the end. Let's add one so we can print it out properly!
  char payload_string[len + 1];
  memset(payload_string, '\0', len + 1);
  memcpy(payload_string, payload, len);

  //update_lcd(String(payload_string), "");

#ifdef CREATURE_DEBUG

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

#endif

  display_message(topic, payload_string);

  digitalWrite(LED_BUILTIN, LOW);
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%I:%M:%S %p");
}

void loop()
{
  
  printLocalTime();
  delay(1000);
}

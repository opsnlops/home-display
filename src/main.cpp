// This isn't gonna work on anything but an ESP32
#if !defined(ESP32)
#error This code is intended to run only on the ESP32 board
#endif

#include <Arduino.h>

#include <WiFi.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include "main.h"

#include <LiquidCrystal.h>
#include <AsyncMqttClient.h>
#include "time.h"

#include <creatures.h>
#include "creature.h"
#include "secrets.h"
#include "mqtt.h"
#include "connection.h"

extern AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// Queue for updates to the display
QueueHandle_t displayQueue;

TaskHandle_t displayUpdateTaskHandler;
TaskHandle_t localTimeTaskHandler;

const int rs = 12, en = 14, d4 = 26, d5 = 25, d6 = 27, d7 = 33;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Clear the entire LCD and print a message
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
  lcd.begin(LCD_WIDTH, LCD_HEIGHT);
  paint_lcd(CREATURE_NAME, "Starting up...");
  delay(1000);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Create the message queue
  displayQueue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(struct DisplayMessage));

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
  mqttClient.onMessage(handle_mqtt_message);
  mqttClient.onPublish(onMqttPublish);

  connectToWiFi();
  digitalWrite(LED_BUILTIN, LOW);

  // TODO: Remove this, don't talk to the Internet
  const char *ntpServer = "pool.ntp.org";
  const long gmtOffset_sec = -28800;
  const int daylightOffset_sec = 3600;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  xTaskCreate(updateDisplayTask,
              "updateDisplayTask",
              2048,
              NULL,
              2,
              &displayUpdateTaskHandler);

  xTaskCreate(printLocalTimeTask,
              "printLocalTimeTask",
              2048,
              NULL,
              1,
              &localTimeTaskHandler);
}

// Stolen from StackOverflow
char *ultoa(unsigned long val, char *s)
{
  char *p = s + 13;
  *p = '\0';
  do
  {
    if ((p - s) % 4 == 2)
      *--p = ',';
    *--p = '0' + val % 10;
    val /= 10;
  } while (val);
  return p;
}

// Since we can allow blanks, let's always send
// a string the length of the display for the bottom
// row
void enqueue_bottom_line(const char *message)
{
  // Construct a DisplayMessage and send it from the ISR-safe handler
  struct DisplayMessage statusUpdate;
  statusUpdate.x = 0;
  statusUpdate.y = 1;

  memset(statusUpdate.text, '\0', LCD_WIDTH + 1);
  memset(statusUpdate.text, ' ', LCD_WIDTH);

  if (strlen(message) < LCD_WIDTH)
  {
    memcpy(statusUpdate.text, message, strlen(message));
  }
  else
  {
    memcpy(statusUpdate.text, message, LCD_WIDTH);
  }

  xQueueSendToBackFromISR(displayQueue, &statusUpdate, NULL);

#ifdef CREATURE_DEBUG
  Serial.print(message);
  Serial.print(" -> ");
  Serial.println(statusUpdate.text);
#endif
}

// Print the temperature we just got from an event. Note that there's
// a bug here if the length of the string is too big, it'll crash.
void print_temperature(const char *room, const char *temperature)
{
  char buffer[LCD_WIDTH + 1];
  memset(buffer, '\0', LCD_WIDTH + 1);
  sprintf(buffer, "%s: %sF", room, temperature);

  enqueue_bottom_line(buffer);
}

// Process a tricky event path to know how to update the display... this
// should be cleaned up when I have the time!
void display_message(const char *topic, const char *message)
{
  int topic_length = strlen(topic);
  if (strncmp(SL_CONNCURRENCY_TOPIC, topic, topic_length) == 0)
  {
    unsigned long concurrency = atol(message);
    char buffer[14];
    char *number_string = ultoa(concurrency, buffer);

    struct DisplayMessage update_message;
    update_message.x = 0;
    update_message.y = 0;
    memcpy(&update_message.text, number_string, sizeof(buffer));

    xQueueSendToBackFromISR(displayQueue, &update_message, NULL);

#ifdef CREATURE_DEBUG
    Serial.print("processed SL concurrency: ");
    Serial.println(ultoa(concurrency, buffer));
#endif
  }
  else if (strncmp(BATHROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      enqueue_bottom_line("Bathroom Motion");
    }
    else
    {
      enqueue_bottom_line("Bathroom Cleared");
    }
  }
  else if (strncmp(BEDROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      enqueue_bottom_line("Bedroom Motion");
    }
    else
    {
      enqueue_bottom_line("Bedroom Cleared");
    }
  }
  else if (strncmp(OFFICE_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      enqueue_bottom_line("Office Motion");
    }
    else
    {
      enqueue_bottom_line("Office Cleared");
    }
  }
  else if (strncmp(LAUNDRY_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      enqueue_bottom_line("Laundry Room Motion");
    }
    else
    {
      enqueue_bottom_line("Laundry Room Cleared");
    }
  }
  else if (strncmp(LIVING_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      enqueue_bottom_line("Living Room Motion");
    }
    else
    {
      enqueue_bottom_line("Living Room Cleared");
    }
  }
  else if (strncmp(WORKSHOP_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      enqueue_bottom_line("Workshop Motion");
    }
    else
    {
      enqueue_bottom_line("Workshop Cleared");
    }
  }
  else if (strncmp(BATHROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Bathroom", message);
  }
  else if (strncmp(BEDROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Bedroom", message);
  }
  else if (strncmp(OFFICE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Office", message);
  }
  else if (strncmp(LAUNDRY_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Laundry", message);
  }
  else if (strncmp(LIVING_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Living", message);
  }
  else if (strncmp(WORKSHOP_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Workshop", message);
  }

  else if (strncmp(OUTSIDE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
  {
    print_temperature("Outside", message);
  }

  else
  {
    Serial.print(topic);
    Serial.print(" ");
    Serial.println(message);

    enqueue_bottom_line(topic);
  }
}

void handle_mqtt_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  digitalWrite(LED_BUILTIN, HIGH);

  // MQTT allows binary content to be passed, so payload will arrive without a
  // NUL at the end. Let's add one so we can print it out properly!
  char payload_string[len + 1];
  memset(payload_string, '\0', len + 1);
  memcpy(payload_string, payload, len);

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

  // Go print the message
  display_message(topic, payload_string);

  digitalWrite(LED_BUILTIN, LOW);
}

void updateDisplayTask(void *pvParamenters)
{

  struct DisplayMessage message;

  for (;;)
  {
    if (displayQueue != NULL)
    {
      if (xQueueReceive(displayQueue, &message, (TickType_t)10) == pdPASS)
      {

        lcd.setCursor(message.x, message.y);
        lcd.print(message.text);

#ifdef CREATURE_DEBUG
        Serial.print("Read message from queue: x: ");
        Serial.print(message.x);
        Serial.print(", y: ");
        Serial.print(message.y);
        Serial.print(", message: '");
        Serial.print(message.text);
        Serial.print("', size: ");
        Serial.println(sizeof(message));
#else
        Serial.print("message read from queue: ");
        Serial.println(message.text);
#endif
      }
    }
    vTaskDelay(TickType_t pdMS_TO_TICKS(100));
  }
}

// Create a task to add the local time to the queue to print
void printLocalTimeTask(void *pvParameters)
{
  struct tm timeinfo;

  for (;;)
  {
    if (getLocalTime(&timeinfo))
    {

      struct DisplayMessage message;
      message.x = 8;
      message.y = 0;

      strftime(message.text, LCD_WIDTH, "%I:%M:%S %p", &timeinfo);

#ifdef CREATURE_DEBUG
      Serial.print("Current time: ");
      Serial.println(message.text);
#endif

      // Drop this message into the queue, giving up after 500ms if there's no
      // space in the queue
      xQueueSendToBack(displayQueue, &message, pdMS_TO_TICKS(500));
    }
    else
    {
      Serial.println("Failed to obtain time");
    }

    // Wait before repeating
    vTaskDelay(TickType_t pdMS_TO_TICKS(1000));
  }
}

void loop()
{
}

// This isn't gonna work on anything but an ESP32
#if !defined(ESP32)
#error This code is intended to run only on the ESP32 board
#endif

#include <Arduino.h>
#include <Wire.h>

#include <WiFi.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include "main.h"

#include <AsyncMqttClient.h>
#include "time.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1325.h>

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

#define OLED_CS 5
#define OLED_RESET 15
#define OLED_DC 13
Adafruit_SSD1325 display(OLED_DC, OLED_RESET, OLED_CS);

// I'll most likely need to figure out a better way to do this, but this will work for now
char clock_display[LCD_WIDTH];
char sl_concurrency[LCD_WIDTH];
char home_message[LCD_WIDTH];
char temperature[LCD_WIDTH];

// Clear the entire LCD and print a message
void paint_lcd(String top_line, String bottom_line)
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(top_line);
  display.print(bottom_line);
  display.display();
}

// Cleanly show an error message
void show_error(String line1, String line2)
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("Oh no! :(");
  display.setTextSize(1);
  display.println("");
  display.println(line1);
  display.println(line2);
  display.display();
}

void WiFiEvent(WiFiEvent_t event)
{
  log_v("[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_WIFI_READY:
    log_d("wifi ready");
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    log_i("WiFi connected");
    log_i("IP address: %s", WiFi.localIP().toString().c_str());
    paint_lcd("My IP", WiFi.localIP().toString());
    connectToMqtt();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    log_w("WiFi lost connection");
    show_error("Unable to connect to Wifi network", getWifiNetwork());
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    onWifiDisconnect(); // Tell the broker we lost Wifi
    break;
    }
}

void set_up_lcd()
{
  log_i("setting up the OLED display");
  display.begin();
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  paint_lcd(CREATURE_NAME, "Starting up...");

  // Initialize our display vars
  memset(clock_display, '\0', LCD_WIDTH);
  memset(sl_concurrency, '\0', LCD_WIDTH);
  memset(home_message, '\0', LCD_WIDTH);
  memset(temperature, '\0', LCD_WIDTH);

  delay(1000);
}

void setup()
{

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
    ;
  log_i("--- STARTED UP ---");

  // Get the display set up
  set_up_lcd();

  // Create the message queue
  displayQueue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(struct DisplayMessage));

  if (!MDNS.begin(CREATURE_NAME))
  {
    log_e("Error setting up mDNS responder!");
    paint_lcd("Unable to set up MDNS", ":(");
    while (1)
    {
      delay(1000);
    }
  }
  log_v("MDNS set up");

  // We aren't _really_ running something on tcp/666, but this lets me
  // find the IP of the creature from an mDNS browser
  MDNS.addService("creature", "tcp", 666);
  MDNS.addServiceTxt("creature", "tcp", "type", "home-display");
  MDNS.addServiceTxt("creature", "tcp", "version", CREATURE_VERSION);
  log_d("added our fake mDNS service");

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWiFi));
  log_d("created the timers");

  WiFi.onEvent(WiFiEvent);
  log_d("setup the Wifi event handler");

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(handle_mqtt_message);
  mqttClient.onPublish(onMqttPublish);
  log_d("set up the MQTT callbacks");

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

// Print the temperature we just got from an event. Note that there's
// a bug here if the length of the string is too big, it'll crash.
void print_temperature(const char *room, const char *temperature)
{
  char buffer[LCD_WIDTH + 1];

  struct DisplayMessage message;
  message.type = temperature_message;

  memset(buffer, '\0', LCD_WIDTH + 1);
  sprintf(message.text, "%s: %sF", room, temperature);

  xQueueSendToBackFromISR(displayQueue, &message, NULL);
}

void show_home_message(const char *message)
{
  struct DisplayMessage home_message;
  home_message.type = home_event_message;

  memset(home_message.text, '\0', LCD_WIDTH + 1);
  memcpy(home_message.text, message, LCD_WIDTH);

  xQueueSendToBackFromISR(displayQueue, &home_message, NULL);
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
    update_message.type = sl_concurrency_message;
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
      show_home_message("Bathroom Motion");
    }
    else
    {
      show_home_message("Bathroom Cleared");
    }
  }
  else if (strncmp(BEDROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      show_home_message("Bedroom Motion");
    }
    else
    {
      show_home_message("Bedroom Cleared");
    }
  }
  else if (strncmp(OFFICE_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      show_home_message("Office Motion");
    }
    else
    {
      show_home_message("Office Cleared");
    }
  }
  else if (strncmp(LAUNDRY_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      show_home_message("Laundry Room Motion");
    }
    else
    {
      show_home_message("Laundry Room Cleared");
    }
  }
  else if (strncmp(LIVING_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      show_home_message("Living Room Motion");
    }
    else
    {
      show_home_message("Living Room Cleared");
    }
  }
  else if (strncmp(WORKSHOP_MOTION_TOPIC, topic, topic_length) == 0)
  {
    if (strncmp(MQTT_ON, message, 2) == 0)
    {
      show_home_message("Workshop Motion");
    }
    else
    {
      show_home_message("Workshop Cleared");
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
    print_temperature("Living Room", message);
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

    show_home_message(topic);
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

        switch (message.type)
        {
        case sl_concurrency_message:
          memcpy(sl_concurrency, message.text, LCD_WIDTH);
          break;
        case home_event_message:
          memcpy(home_message, message.text, LCD_WIDTH);
          break;
        case clock_display_message:
          memcpy(clock_display, message.text, LCD_WIDTH);
          break;
        case temperature_message:
          memcpy(temperature, message.text, LCD_WIDTH);
        }

        // The display is buffered, so this just means wipe out what's there
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.println(sl_concurrency);
        display.setTextSize(1);
        display.println("");
        display.println(home_message);
        display.println(temperature);
        display.println("");
        display.println("");
        display.print("          ");
        display.println(clock_display);
        display.display();

#ifdef CREATURE_DEBUG
        Serial.print("Read message from queue: type: ");
        Serial.print(message.type);
        Serial.print(", text: ");
        Serial.print(message.text);
        Serial.print(", size: ");
        Serial.println(sizeof(message));
#else
        log_d("message read from queue: %s", message.text);
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
      message.type = clock_display_message;

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
      log_w("Failed to obtain time");
    }

    // Wait before repeating
    vTaskDelay(TickType_t pdMS_TO_TICKS(1000));
  }
}

void loop()
{
}


#ifndef _APRILS_CREATRES_HOME_DISPLAY
#define _APRILS_CREATRES_HOME_DISPLAY

#include <WiFi.h>

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

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include <AsyncMqttClient.h>

#define LCD_WIDTH 22
#define DISPLAY_QUEUE_LENGTH 5


enum MessageType {
  clock_display_message,
  sl_concurrency_message,
  home_event_message,
  temperature_message
};


// One message for the display
struct DisplayMessage
{
  MessageType type;
  char text[LCD_WIDTH + 1];
} __attribute__((packed));


void WiFiEvent(WiFiEvent_t event);

char *ultoa(unsigned long val, char *s);
void set_up_lcd();
void show_startup(String line1);
void show_error(String line1, String line2);
void paint_lcd(String top_line, String bottom_line);
void updateDisplayTask(void *pvParamenters);
void printLocalTimeTask(void *pvParameters);

void print_temperature(const char *room, const char *temperature);
void display_message(const char *topic, const char *message);

void handle_mqtt_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

portTASK_FUNCTION_PROTO(messageQueueReaderTask, pvParameters);

#endif
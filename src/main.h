
#pragma once

#include <WiFi.h>


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
  flamethrower_message,
  home_event_message,
  temperature_message
};


// One message for the display
struct DisplayMessage
{
  MessageType type;
  char text[LCD_WIDTH + 1];
} __attribute__((packed));


char *ultoa(unsigned long val, char *s);
void set_up_lcd();
void show_startup(String line1);
void show_error(String line1, String line2);
void paint_lcd(String top_line, String bottom_line);
void printLocalTimeTask(void *pvParameters);
void updateHouseStatus();
void print_flamethrower(const char *room, boolean on);

void print_temperature(const char *room, const char *temperature);
void display_message(const char *topic, const char *message);

void handle_mqtt_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

portTASK_FUNCTION_PROTO(messageQueueReaderTask, pvParameters);
portTASK_FUNCTION_PROTO(updateDisplayTask, pvParameters);

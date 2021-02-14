
#ifndef _APRILS_CREATRES_HOME_DISPLAY
#define _APRILS_CREATRES_HOME_DISPLAY

#include <WiFi.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include <AsyncMqttClient.h>

#define LCD_HEIGHT 2
#define LCD_WIDTH 16
#define DISPLAY_QUEUE_LENGTH 5

// One message for the display
struct DisplayMessage
{
  uint8_t x;
  uint8_t y;
  char text[LCD_WIDTH + 1];
} __attribute__((packed));


void WiFiEvent(WiFiEvent_t event);

char *ultoa(unsigned long val, char *s);
void set_up_lcd();
void paint_lcd(String top_line, String bottom_line);
void updateDisplayTask(void *pvParamenters);
void printLocalTimeTask(void *pvParameters);

void enqueue_bottom_line(const char *message);
void print_temperature(const char *room, const char *temperature);
void display_message(const char *topic, const char *message);

void handle_mqtt_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

#endif
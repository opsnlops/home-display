// This isn't gonna work on anything but an ESP32
#if !defined(ESP32)
#error This code is intended to run only on the ESP32 board
#endif

/*
    Wiring Notes!

    These are GPIO numbers, not pin numbers.

    ESP32           OLED          What
    15              16            Reset
    17              4             DC (Data / Command)
    5               15            CS (Chip Select)
    18              7             Clock
    23              8             Data In


    Also note that it runs on v3.3, not v5.

*/

#include <Arduino.h>
#include <Wire.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include "main.h"
#include "config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1325.h>

#include "creature.h"

#include "logging/logging.h"
#include "network/connection.h"
#include "creatures/creatures.h"
#include "mqtt/mqtt.h"
#include "time/time.h"
#include "mdns/creature-mdns.h"
#include "mdns/magicbroker.h"
#include "home/data-feed.h"

#include "ota.h"

using namespace creatures;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// Queue for updates to the display
QueueHandle_t displayQueue;

TaskHandle_t displayUpdateTaskHandler;
TaskHandle_t localTimeTaskHandler;

uint8_t startup_counter = 0;

#define OLED_CS 5
#define OLED_RESET 15
#define OLED_DC 17
Adafruit_SSD1325 display(OLED_DC, OLED_RESET, OLED_CS);

// I'll most likely need to figure out a better way to do this, but this will work for now
char home_status[LCD_WIDTH];
char flamethrower_display[LCD_WIDTH];
char clock_display[LCD_WIDTH];
char home_message[LCD_WIDTH];
char temperature[LCD_WIDTH];

// Current state of things
float gOutsideTemperature = 32.0;
float gWindSpeed = 0.0;
float gHousePower = 500.0;

/*
    Configuration!

    This isn't saved anywhere on the MCU itself. All of the config is kept in a retained
    MQTT topic, which is read when the Creature boots.

    This is why where's "local" and "global" topics in MQTT. The config is local and is
    keyed to CREATURE_NAME.

*/
boolean gDisplayOn = true;

// Keep a link to our logger
static Logger l;
static MQTT mqtt = MQTT(String(CREATURE_NAME));

// Clear the entire LCD and print a message
void paint_lcd(String top_line, String bottom_line)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(top_line);
    display.print(bottom_line);
    display.display();
}

void __show_big_message(String header, String line1, String line2)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println(header);
    display.setTextSize(1);
    display.println("");
    display.println(line1);
    display.println(line2);
    display.display();
}

// Cleanly show an error message
void show_error(String line1, String line2)
{
    __show_big_message("Oh no! :(", line1, line2);
}

// Cleanly show an error message
void show_startup(String line1)
{
    char buffer[3] = {'\0', '\0', '\0'};
    itoa(startup_counter++, buffer, 10);
    __show_big_message("Booting...", buffer, line1);
}

void set_up_lcd()
{
    l.info("setting up the OLED display");
    display.begin();
    display.display();
    delay(250);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    paint_lcd(CREATURE_NAME, "Starting up...");

    // Initialize our display vars
    memset(home_status, '\0', LCD_WIDTH);
    memset(clock_display, '\0', LCD_WIDTH);
    memset(home_message, '\0', LCD_WIDTH);
    memset(temperature, '\0', LCD_WIDTH);
    memset(flamethrower_display, '\0', LCD_WIDTH);

    delay(1000);
}

void setup()
{
    // Fire up the logging framework first
    l.init();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    l.info("--- STARTED UP ---");

    // Get the display set up
    set_up_lcd();
    show_startup("display set up");

    show_startup("starting up Wifi");
    NetworkConnection network = NetworkConnection();
    if (!network.connectToWiFi())
    {
        show_error("I can't WiFi :(", "Check provisioning!");
        while (1)
            ;
    }

    // Create the message queue
    displayQueue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(struct DisplayMessage));
    l.debug("displayQueue made");

    // Register ourselves in mDNS
    show_startup("Starting in mDNS");
    CreatureMDNS creatureMDNS = CreatureMDNS(CREATURE_NAME, CREATURE_POWER);
    creatureMDNS.registerService(666);
    creatureMDNS.addStandardTags();

    // Set the time
    show_startup("Setting time");
    Time time = Time();
    time.init();
    time.obtainTime();

    // Get the location of the magic broker
    show_startup("Finding the broker");
    MagicBroker magicBroker;
    magicBroker.find();

    // Connect to MQTT
    show_startup("Starting MQTT");
    mqtt.connect(magicBroker.ipAddress, magicBroker.port);
    mqtt.subscribe(String("cmd"), 0);
    mqtt.subscribe(String("config"), 0);

    mqtt.subscribeGlobalNamespace(OUTSIDE_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OUTSIDE_WIND_SPEED_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(HOME_POWER_USE_WATTS, 0);

    mqtt.subscribeGlobalNamespace(BUNNYS_ROOM_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(FAMILY_ROOM_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(GUEST_ROOM_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(KITCHEN_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OFFICE_TEMPERATURE_TOPIC, 0);

    mqtt.subscribeGlobalNamespace(FAMILY_ROOM_FLAMETHROWER_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OFFICE_FLAMETHROWER_TOPIC, 0);

    // mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    // wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWiFi));
    // l.debug("created the timers");
    // show_startup("timers made");

    xTaskCreatePinnedToCore(updateDisplayTask,
                            "updateDisplayTask",
                            10240,
                            NULL,
                            2,
                            &displayUpdateTaskHandler,
                            1);

    xTaskCreate(printLocalTimeTask,
                "printLocalTimeTask",
                2048,
                NULL,
                1,
                &localTimeTaskHandler);

    show_startup("tasks running");

    // Enable OTA
    setup_ota(String(CREATURE_NAME));
    start_ota();

    // Tell MQTT we're alive
    mqtt.publish(String("status"), String("I'm alive!!"), 0, false);
    mqtt.startHeartbeat();

    digitalWrite(LED_BUILTIN, LOW);

    // Start the task to read the queue
    l.debug("starting the message reader task");
    TaskHandle_t messageReaderTaskHandle;
    xTaskCreatePinnedToCore(messageQueueReaderTask,
                            "messageQueueReaderTask",
                            10240,
                            NULL,
                            1,
                            &messageReaderTaskHandle,
                            1);
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
    l.debug("printing temperature, room: %s", room);
    char buffer[LCD_WIDTH + 1];

    struct DisplayMessage message;
    message.type = temperature_message;

    memset(buffer, '\0', LCD_WIDTH + 1);
    sprintf(message.text, "%s: %sF", room, temperature);

    xQueueSendToBackFromISR(displayQueue, &message, NULL);
}

void print_flamethrower(const char *room, boolean on)
{
    l.debug("printing flamethrower, room: %s", room);
    char buffer[LCD_WIDTH + 1];

    char *action = "Off";
    if (on)
        action = "On";

    struct DisplayMessage message;
    message.type = flamethrower_message;

    memset(buffer, '\0', LCD_WIDTH + 1);
    sprintf(message.text, "%s %s", room, action);

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
    if (strncmp(HALLWAY_BATHROOM_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Hallway Bathroom Motion");
        }
        else
        {
            show_home_message("Hallway Bathroom Cleared");
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
    else if (strncmp(HALF_BATHROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Half Bathroom", message);
    }
    else if (strncmp(BUNNYS_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Bunny's Room", message);
    }
    else if (strncmp(OFFICE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Office", message);
    }
    else if (strncmp(FAMILY_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Family Room", message);
    }
    else if (strncmp(WORKSHOP_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Workshop", message);
    }
    else if (strncmp(GUEST_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Guest Room", message);
    }
    else if (strncmp(KITCHEN_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Kitchen", message);
    }

    else if (strncmp(OUTSIDE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        gOutsideTemperature = atof(message);
        updateHouseStatus();
    }

    else if (strncmp(OUTSIDE_WIND_SPEED_TOPIC, topic, topic_length) == 0)
    {
        gWindSpeed = atof(message);
        updateHouseStatus();
    }

    else if (strncmp(HOME_POWER_USE_WATTS, topic, topic_length) == 0)
    {
        gHousePower = atof(message);
        updateHouseStatus();
    }

    else if (strncmp(FAMILY_ROOM_FLAMETHROWER_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp("false", message, strlen(message)) == 0)
        {
            print_flamethrower("Family Room", false);
        }
        else
        {
            print_flamethrower("Family Room", true);
        }
    }
    else if (strncmp(OFFICE_FLAMETHROWER_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp("false", message, strlen(message)) == 0)
        {
            print_flamethrower("Office", false);
        }
        else
        {
            print_flamethrower("Office", true);
        }
    }
    else
    {
        l.warning("Unknown message! topic: %s, message %s", topic, message);
        // show_home_message(topic);
    }
}

void updateHouseStatus()
{
    // Update the array that holds the information for the overall status
    sprintf(home_status, "%.1fF %.1fMPH %.1fW", gOutsideTemperature, gWindSpeed, gHousePower);
}

portTASK_FUNCTION(updateDisplayTask, pvParameters)
{

    struct DisplayMessage message;

    for (;;)
    {
        if (displayQueue != NULL)
        {
            if (xQueueReceive(displayQueue, &message, (TickType_t)10) == pdPASS)
            {

                if (message.text != NULL)
                {
                    l.verbose("got a message: %s", message.text);
                    switch (message.type)
                    {
                    case home_event_message:
                        memcpy(home_message, message.text, LCD_WIDTH);
                        break;
                    case flamethrower_message:
                        memcpy(flamethrower_display, message.text, LCD_WIDTH);
                        break;
                    case clock_display_message:
                        memcpy(clock_display, message.text, LCD_WIDTH);
                        break;
                    case temperature_message:
                        memcpy(temperature, message.text, LCD_WIDTH);
                    }

                    // The display is buffered, so this just means wipe out what's there
                    display.clearDisplay();

                    // If the display is off, don't show anything
                    if (gDisplayOn)
                    {
                        display.setCursor(0, 0);
                        display.setTextSize(1);
                        display.println(home_status);
                        display.println("");
                        display.println(flamethrower_display);
                        display.println(home_message);
                        display.println(temperature);
                        display.println("");
                        display.println("");
                        display.print("          ");
                        display.println(clock_display);
                    }

                    // Update!
                    display.display();
                }
                else
                {
                    l.warning("NULL message pulled from the displayQueue");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
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
            l.verbose("Current time: %s", message.text);

            // Drop this message into the queue, giving up after 500ms if there's no
            // space in the queue
            xQueueSendToBack(displayQueue, &message, pdMS_TO_TICKS(500));
        }
        else
        {
            l.warning("Failed to obtain time");
        }

        // Wait before repeating
        vTaskDelay(TickType_t pdMS_TO_TICKS(1000));
    }
}

// Read from the queue and print it to the screen for now
portTASK_FUNCTION(messageQueueReaderTask, pvParameters)
{

    QueueHandle_t incomingQueue = mqtt.getIncomingMessageQueue();
    for (;;)
    {
        struct MqttMessage message;
        if (xQueueReceive(incomingQueue, &message, (TickType_t)5000) == pdPASS)
        {
            l.debug("Incoming message! local topic: %s, global topic: %s, payload: %s",
                    message.topic,
                    message.topicGlobalNamespace,
                    message.payload);

            // Is this a config message?
            if (strncmp("config", message.topic, strlen(message.topic)) == 0)
            {
                l.info("Got a config message from MQTT: %s", message.payload);
                updateConfig(String(message.payload));
            }
            else
            {
                display_message(message.topic, message.payload);
            }
        }
    }
}

void loop()
{
    vTaskDelete(NULL);
}

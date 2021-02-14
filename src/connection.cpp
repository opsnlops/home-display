
// This isn't gonna work on anything but an ESP32
#if !defined(ESP32)
#error This code is intended to run only on the ESP32 board
#endif

#include <WiFi.h>

#include "connection.h"
#include "secrets.h"

const int LED_PIN = LED_BUILTIN;


void connectToWiFi()
{
    Serial.print("Connecting to WiFi network: ");
    Serial.print(WIFI_NETWORK);
    Serial.print("...");

    WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);
}

/**
 * This tosses SOS onto the internal LED. It's a dead loop, and means we couldn't connect.
 */
void signal_sos()
{
    while (true)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(400);

        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(400);

        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(2000);
    }
}

IPAddress find_broker(const char *broker_service, const char *broker_protocol)
{

    Serial.printf("Browsing for service _%s._%s.local. ... ", broker_service, broker_protocol);

    int n = MDNS.queryService(broker_service, broker_protocol);
    if (n == 0)
    {
        Serial.println("no services found");
    }
    else
    {
        Serial.print(n);
        Serial.println(" service(s) found");
        for (int i = 0; i < n; ++i)
        {
            // Print details for each service found
            Serial.print("  ");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(MDNS.hostname(i));
            Serial.print(" (");
            Serial.print(MDNS.IP(i));
            Serial.print(":");
            Serial.print(MDNS.port(i));
            Serial.print(") ");
            Serial.println(MDNS.txt(i, 0));

            // hahah
            return MDNS.IP(i);
        }
    }

    // This shouldn't happen, but provide a safe default
    return DEFAULT_IP_ADDRESS;
}

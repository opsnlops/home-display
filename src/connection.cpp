
// This isn't gonna work on anything but an ESP32
#if !defined(ESP32)
#error This code is intended to run only on the ESP32 board
#endif

#include <WiFi.h>

extern "C"
{
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
}

#include "main.h"
#include "connection.h"
#include "secrets.h"

const int LED_PIN = LED_BUILTIN;

char *getWifiNetwork()
{
    return WIFI_NETWORK;
}

void connectToWiFi()
{
    log_i("Connecting to WiFi network: %s", WIFI_NETWORK);
    show_startup("about to connect to Wifi");

    // The brownout detector is _really_ touchy while starting up Wifi,
    // turn it off just for a moment to set the Wifi chip start up
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);

    // ...and now turn it back on
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);

    show_startup("WiFi.begin() done");
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

    //log_v("returning a fake broker IP");
    //return IPAddress(192, 168, 7, 129);

    log_i("Browsing for service _%s._%s.local. ... ", broker_service, broker_protocol);

    int n = MDNS.queryService(broker_service, broker_protocol);
    if (n == 0)
    {
        log_w("couldn't find the magic broker in mDNS");
    }
    else
    {
        log_i("%d services(s) found", n);
        for (int i = 0; i < n; ++i)
        {
            // Print details for each service found
            log_d("  %d: %s (%s:%d) role: %s", (i + 1), MDNS.hostname(i), MDNS.IP(i).toString().c_str(), MDNS.port(i), MDNS.txt(i, 0));

            // hahah
            return MDNS.IP(i);
        }
    }

    // This shouldn't happen, but provide a safe default
    return DEFAULT_IP_ADDRESS;
}

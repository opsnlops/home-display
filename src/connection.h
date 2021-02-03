
#ifndef _APRILS_CREATURES_CONNECTION
#define _APRILS_CREATURES_CONNECTION

#include <WiFi.h>
#include <ESPmDNS.h>

void connectToWiFi(const char *ssid, const char *pwd);
void signal_conenction_error();

IPAddress find_broker(const char *broker_service, const char *broker_protocol);

#endif
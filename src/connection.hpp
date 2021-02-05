
#ifndef _APRILS_CREATURES_CONNECTION
#define _APRILS_CREATURES_CONNECTION

#include <WiFi.h>
#include <ESPmDNS.h>


const IPAddress DEFAULT_IP_ADDRESS = IPAddress(0,0,0,0);

void connectToWiFi();
void signal_conenction_error();

IPAddress find_broker(const char *broker_service, const char *broker_protocol);

#endif
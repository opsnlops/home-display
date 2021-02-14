

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

void updateDisplayTask(void *pvParamenters);
void printLocalTimeTask(void *pvParameters);
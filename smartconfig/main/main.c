#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_smartconfig.h"

void app_main(void)
{
    initialise_wifi();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

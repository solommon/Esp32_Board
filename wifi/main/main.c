#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "simple_wifi_sta.h"

void app_main(void)
{
    wifi_sta_init();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

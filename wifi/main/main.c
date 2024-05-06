#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "simple_wifi_sta.h"
#include "nvs_flash.h"
#include "nvs.h"

void app_main(void)
{
    nvs_flash_init();
    wifi_sta_init();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

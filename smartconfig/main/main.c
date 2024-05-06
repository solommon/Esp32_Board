#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_smartconfig.h"
#include "nvs_flash.h"

void app_main(void)
{
    nvs_flash_init();
    initialise_wifi();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

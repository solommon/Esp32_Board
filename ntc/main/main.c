/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ntc.h"

#define TAG     "main"

void app_main(void)
{
    temp_ntc_init();
    while(1)
    {
        ESP_LOGI(TAG,"current temp:%.2f",get_temp());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

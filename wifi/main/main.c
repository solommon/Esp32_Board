#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "simple_wifi_sta.h"
#include "nvs_flash.h"
#include "nvs.h"

void app_main(void)
{
    //NVS初始化（WIFI底层驱动有用到NVS，所以这里要初始化）
    nvs_flash_init();
    //wifi STA工作模式初始化
    wifi_sta_init();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

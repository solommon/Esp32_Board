#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lv_port.h"
#include "lvgl.h"
#include "lv_demos.h"

// 主函数
void app_main(void)
{
	lv_port_init();
    lv_demo_widgets();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
}

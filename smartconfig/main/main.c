#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_smartconfig.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "button.h"

//按键事件组
static EventGroupHandle_t s_pressEvent;
#define SHORT_EV    BIT0    //短按
#define LONG_EV     BIT1    //长按
#define BTN_GPIO    GPIO_NUM_39

/** 长按按键回调函数
 * @param 无
 * @return 无
*/
void long_press_handle(void)
{
    xEventGroupSetBits(s_pressEvent,LONG_EV);
}


void app_main(void)
{
    nvs_flash_init();           //初始化NVS
    initialise_wifi();          //初始化wifi

    s_pressEvent = xEventGroupCreate();
    button_config_t btn_cfg = 
    {
        .gpio_num = BTN_GPIO,       //gpio号
        .active_level = 0,          //按下的电平
        .long_press_time = 3000,    //长按时间
        .short_cb = NULL,           //短按回调函数
        .long_cb = smartconfig_start             //长按回调函数
    };
    button_event_set(&btn_cfg);     //添加按键响应事件处理
    EventBits_t ev;

    while(1)
    {
        ev = xEventGroupWaitBits(s_pressEvent,LONG_EV,pdTRUE,pdFALSE,portMAX_DELAY);
        if(ev & LONG_EV)
        {
            smartconfig_start();    //检测到长按事件，启动smartconfig
        }
    }
}

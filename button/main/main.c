#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "button.h"

#define BTN_GPIO    GPIO_NUM_39
#define LED_GPIO    GPIO_NUM_27

/** 简单例程
 * @param 无
 * @return 无
*/
void simple_btn_test(void* arg)
{
    //初始化按键GPIO
    gpio_config_t gpio_t = 
    {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ull<<BTN_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_t));
    while(1)
    {
        //检测到按键按下，就亮，否则熄灭
        if(gpio_get_level(BTN_GPIO) == 0)
            gpio_set_level(LED_GPIO,1);
        else
            gpio_set_level(LED_GPIO,0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//按键事件组
static EventGroupHandle_t s_pressEvent;
#define SHORT_EV    BIT0    //短按
#define LONG_EV     BIT1    //长按

/** 短按按键回调函数
 * @param 无
 * @return 无
*/
void short_press_handle(void)
{
    xEventGroupSetBits(s_pressEvent,SHORT_EV);
}

/** 长按按键回调函数
 * @param 无
 * @return 无
*/
void long_press_handle(void)
{
    xEventGroupSetBits(s_pressEvent,LONG_EV);
}


/** 完整的按键+LED演示程序
 * @param 无
 * @return 无
*/
void complete_btn_test(void* arg)
{
    s_pressEvent = xEventGroupCreate();
    button_config_t btn_cfg = 
    {
        .gpio_num = BTN_GPIO,       //gpio号
        .active_level = 0,          //按下的电平
        .long_press_time = 3000,    //长按时间
        .short_cb = short_press_handle,           //短按回调函数
        .long_cb = long_press_handle             //长按回调函数
    };
    button_event_set(&btn_cfg);     //添加按键响应事件处理
    EventBits_t ev;
    while(1)
    {
        //等待按键按下事件
        ev = xEventGroupWaitBits(s_pressEvent,SHORT_EV|LONG_EV,pdTRUE,pdFALSE,portMAX_DELAY);
        if(ev & SHORT_EV)
        {
            //短按事件，亮一下熄灭
            gpio_set_level(LED_GPIO,1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_GPIO,0);
        }
        if(ev & LONG_EV)
        {
            //长按事件，闪烁8下，然后熄灭
            for(int i = 0;i < 8;i++)
            {
                gpio_set_level(LED_GPIO,1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_GPIO,0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
        xEventGroupClearBits(s_pressEvent,SHORT_EV);
        xEventGroupClearBits(s_pressEvent,LONG_EV);
    }
}

void app_main(void)
{
    //初始化LED的GPIO管脚，设置成输出
    gpio_config_t gpio_t = 
    {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ull<<BTN_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_t));

    //简单按键例程
    xTaskCreatePinnedToCore(simple_btn_test,"btn1",2048,NULL,3,NULL,1);

    //较通用的按键例程
    xTaskCreatePinnedToCore(complete_btn_test,"btn2",2048,NULL,3,NULL,1);
}

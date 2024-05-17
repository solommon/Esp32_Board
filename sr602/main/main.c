#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char* TAG = "main";

#define LED_GPIO    GPIO_NUM_27
#define SR602_GPIO  GPIO_NUM_34

void app_main(void)
{
    gpio_config_t led_gpio = 
    {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pin_bit_mask = (1ull<<LED_GPIO)
    };
    gpio_config(&led_gpio);
    
    gpio_set_level(LED_GPIO,0);

    gpio_config_t sr602_gpio = 
    {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pin_bit_mask = (1ull<<SR602_GPIO)
    };
    gpio_config(&sr602_gpio);

    while(1)
    {
        if(gpio_get_level(SR602_GPIO))
            gpio_set_level(LED_GPIO,1);
        else
            gpio_set_level(LED_GPIO,0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

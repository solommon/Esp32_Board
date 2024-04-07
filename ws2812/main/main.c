/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_ws2812.h"

#define WS2812_GPIO_NUM     GPIO_NUM_26
#define WS2812_LED_NUM      12

//hsv转RGB
//h色调 0-360
//s饱和度 0-100
//v亮度 0-100
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

#define DEFAULT_H   50
#define DEFAULT_S   50

uint8_t get_brightness(int index)
{
    //亮度数组    
    static const uint8_t wave_brightness[] = 
    {
        0,0,0,0,0,
        10,10,10,10,10,10,10,10,10,10,
        15,15,15,15,15,15,15,15,15,
        20,20,20,20,20,20,20,20,
        25,25,25,25,25,25,25,
        30,30,30,30,30,30,30,
        45,45,45,45,45,45,
        50,50,50,50,50,
        55,55,55,55,55,
        60,60,60,60,
        65,65,65,65,
        70,70,70,
        65,65,65,65,
        60,60,60,60,
        55,55,55,55,55,
        50,50,50,50,50,
        45,45,45,45,45,45,
        30,30,30,30,30,30,30,
        25,25,25,25,25,25,25,
        20,20,20,20,20,20,20,20,
        15,15,15,15,15,15,15,15,15,
        10,10,10,10,10,10,10,10,10,10,
        0,0,0,0,0,
    };

    #define ARRAY_OFFSET    0

    //下标数组
    static uint8_t brightness_index[WS2812_LED_NUM] = 
    {
        11+ARRAY_OFFSET*10,
        10+ARRAY_OFFSET*9,
        9+ARRAY_OFFSET*8,
        8+ARRAY_OFFSET*7,
        7+ARRAY_OFFSET*6,
        6+ARRAY_OFFSET*5,
        5+ARRAY_OFFSET*4,
        4+ARRAY_OFFSET*3,
        3+ARRAY_OFFSET*2,
        2+ARRAY_OFFSET*1,
        1+ARRAY_OFFSET*0,
        0+ARRAY_OFFSET*11,
    };

    //找到亮度值
    uint8_t brightness = wave_brightness[brightness_index[index]];
    brightness_index[index]++;
    if(brightness_index[index] >= sizeof(wave_brightness))
    {
        brightness_index[index] = 0;
    }

    return brightness;
}

void app_main(void)
{
    ws2812_strip_t *ws2812_handle = NULL;
    int index = 0;
    ws2812_init(WS2812_GPIO_NUM,WS2812_LED_NUM,&ws2812_handle);

    while(1)
    {
        for(index = 0;index < WS2812_LED_NUM;index++)
        {
            uint32_t r,g,b;
            led_strip_hsv2rgb(DEFAULT_H,DEFAULT_S,get_brightness(index),&r,&g,&b);
            ws2812_write(ws2812_handle,index,r,g,b);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

}

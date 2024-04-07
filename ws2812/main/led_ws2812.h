/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    rmt_channel_handle_t led_chan;          //rmt通道
    rmt_encoder_handle_t led_encoder;       //rmt编码器
    uint8_t *led_buffer;                    //rgb数据
    int led_num;                            //led个数
}ws2812_strip_t;

esp_err_t ws2812_init(gpio_num_t gpio,int maxled,ws2812_strip_t** led_handle);
esp_err_t ws2812_deinit(ws2812_strip_t* handle);
esp_err_t ws2812_write(ws2812_strip_t* handle,uint32_t index,uint32_t r,uint32_t g,uint32_t b);


#ifdef __cplusplus
}
#endif

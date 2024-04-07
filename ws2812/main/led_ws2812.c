/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "led_ws2812.h"
#include "driver/rmt_tx.h"

static const char *TAG = "led_encoder";

#define LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    /*
    __containerof宏的作用:
    通过结构的成员来访问这个结构的地址
    在这个函数中，传入参数encoder是rmt_led_strip_encoder_t结构体中的base成员
    __containerof宏通过encoder的地址，根据rmt_led_strip_encoder_t的内存排布找到rmt_led_strip_encoder_t* 的首地址
    */
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    switch (led_encoder->state) {   //led_encoder->state是自定义的状态，这里只有两种值，0是发送RGB数据，1是发送复位码
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

esp_err_t rmt_new_led_strip_encoder(rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;

    //创建一个自定义的编码器，用于控制发送编码的流程
    rmt_led_strip_encoder_t *led_encoder = NULL;
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;    //这个函数会在rmt发送数据的时候被调用，我们可以在这个函数增加额外代码进行控制
    led_encoder->base.del = rmt_del_led_strip_encoder;  //这个函数在卸载rmt时被调用
    led_encoder->base.reset = rmt_led_strip_encoder_reset;  //这个函数在复位rmt时被调用

    //新建一个编码器配置(0,1位持续时间，参考芯片手册)
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * LED_STRIP_RESOLUTION_HZ / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * LED_STRIP_RESOLUTION_HZ / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * LED_STRIP_RESOLUTION_HZ / 1000000, // T1H=0.9us
            .level1 = 0,
            .duration1 = 0.3 * LED_STRIP_RESOLUTION_HZ / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    //传入编码器配置，获得数据编码器操作句柄
    rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder);

    //新建一个拷贝编码器配置，拷贝编码器一般用于传输恒定的字符数据，比如说结束位
    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder);

    //设定结束位时序
    uint32_t reset_ticks = LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2; //分辨率/1M=每个ticks所需的us，然后乘以50就得出50us所需的ticks
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };

    //返回编码器
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}

esp_err_t ws2812_init(gpio_num_t gpio,int maxled,ws2812_strip_t** handle)
{
    ws2812_strip_t* led_handle = NULL;
    led_handle = calloc(1, sizeof(ws2812_strip_t));
    assert(led_handle);
    led_handle->led_buffer = calloc(1,maxled*3);
    assert(led_handle->led_buffer);
    led_handle->led_num = maxled;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,         //默认时钟源
        .gpio_num = gpio,                       //GPIO管脚
        .mem_block_symbols = 64,                //缓存
        .resolution_hz = LED_STRIP_RESOLUTION_HZ,   //RMT通道的分辨率10000000hz=0.1us
        .trans_queue_depth = 4,                 //底层后台发送的队列深度
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_handle->led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&led_handle->led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_handle->led_chan));

    *handle = led_handle;

    return ESP_OK;
}

esp_err_t ws2812_deinit(ws2812_strip_t* handle)
{
    if(!handle)
        return ESP_OK;
    rmt_del_encoder(handle->led_encoder);
    if(handle->led_buffer)
        free(handle->led_buffer);
    free(handle);
    return ESP_OK;
}

esp_err_t ws2812_write(ws2812_strip_t* handle,uint32_t index,uint32_t r,uint32_t g,uint32_t b)
{
     rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    if(index >= handle->led_num)
        return ESP_FAIL;
    uint32_t start = index*3;
    handle->led_buffer[start+0] = r & 0xff;
    handle->led_buffer[start+1] = g & 0xff;
    handle->led_buffer[start+2] = b & 0xff;

    return rmt_transmit(handle->led_chan, handle->led_encoder, handle->led_buffer, handle->led_num*3, &tx_config);
    
}


/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "driver/mcpwm_cap.h"
#include "driver/gpio.h"

const static char *TAG = "example";

//TRIG和ECHO对应的GPIO
#define HC_SR04_TRIG_GPIO  GPIO_NUM_32
#define HC_SR04_ECHO_GPIO  GPIO_NUM_33

static bool hc_sr04_echo_callback(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data)
{
    static uint32_t cap_val_begin_of_sample = 0;
    static uint32_t cap_val_end_of_sample = 0;
    TaskHandle_t task_to_notify = (TaskHandle_t)user_data;
    BaseType_t high_task_wakeup = pdFALSE;

    //判断边沿，上升沿记录捕获的计数值
    if (edata->cap_edge == MCPWM_CAP_EDGE_POS) {
        // store the timestamp when pos edge is detected
        cap_val_begin_of_sample = edata->cap_value;
        cap_val_end_of_sample = cap_val_begin_of_sample;
    } else {
        //如果是下降沿，也记录捕获的计数值
        cap_val_end_of_sample = edata->cap_value;

        //两个计数值的差值，就是脉宽长度
        uint32_t tof_ticks = cap_val_end_of_sample - cap_val_begin_of_sample;

        // 通知任务计数差值
        xTaskNotifyFromISR(task_to_notify, tof_ticks, eSetValueWithOverwrite, &high_task_wakeup);
    }

    return high_task_wakeup == pdTRUE;
}

/**
 * @brief generate single pulse on Trig pin to start a new sample
 */
static void gen_trig_output(void)
{
    gpio_set_level(HC_SR04_TRIG_GPIO, 1); // set high
    esp_rom_delay_us(10);
    gpio_set_level(HC_SR04_TRIG_GPIO, 0); // set low
}

void app_main(void)
{
    ESP_LOGI(TAG, "Install capture timer");

    //新建一个捕获定时器，使用默认的时钟源
    mcpwm_cap_timer_handle_t cap_timer = NULL;
    mcpwm_capture_timer_config_t cap_conf = {
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_capture_timer(&cap_conf, &cap_timer));

    //新建一个捕获通道，把捕获定时器与捕获通道绑定起来，采取双边捕获的策略
    ESP_LOGI(TAG, "Install capture channel");
    mcpwm_cap_channel_handle_t cap_chan = NULL;
    mcpwm_capture_channel_config_t cap_ch_conf = {
        .gpio_num = HC_SR04_ECHO_GPIO,
        .prescale = 1,
        // capture on both edge
        .flags.neg_edge = true,
        .flags.pos_edge = true,
        // pull up internally
        .flags.pull_up = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_capture_channel(cap_timer, &cap_ch_conf, &cap_chan));

    //新建一个捕获事件回调函数，当捕获到相关的边沿，则调用这个回调函数
    ESP_LOGI(TAG, "Register capture callback");
    TaskHandle_t cur_task = xTaskGetCurrentTaskHandle();
    mcpwm_capture_event_callbacks_t cbs = {
        .on_cap = hc_sr04_echo_callback,
    };
    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(cap_chan, &cbs, cur_task));

    //使能捕获通道
    ESP_LOGI(TAG, "Enable capture channel");
    ESP_ERROR_CHECK(mcpwm_capture_channel_enable(cap_chan));

    //初始化Trig引脚，设置为输出
    ESP_LOGI(TAG, "Configure Trig pin");
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << HC_SR04_TRIG_GPIO,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(HC_SR04_TRIG_GPIO, 0));

    //使能捕获定时器
    ESP_LOGI(TAG, "Enable and start capture timer");
    ESP_ERROR_CHECK(mcpwm_capture_timer_enable(cap_timer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_start(cap_timer));

    uint32_t tof_ticks;
    while (1) {
        //产生一个Trig
        gen_trig_output();

        // 等待捕获完成信号，
        if (xTaskNotifyWait(0x00, ULONG_MAX, &tof_ticks, pdMS_TO_TICKS(1000)) == pdTRUE) {
            //计算脉宽时间长度，从任务通知获取到的tof_ticks是计数，我们需要计算出对应的时间
            /*
            tof_ticks:计数
            esp_clk_apb_freq():计数时钟频率
            1/esp_clk_apb_freq():计数时钟周期，也就是1个tof_ticks时间是多少秒
            1000000/esp_clk_apb_freq():一个tof_ticks时间是多少us
            因此tof_ticks * (1000000.0 / esp_clk_apb_freq())就是总的脉宽时长
            */
            float pulse_width_us = tof_ticks * (1000000.0 / esp_clk_apb_freq());
            if (pulse_width_us > 35000) {
                // 脉宽太长，超出了SR04的计算范围
                continue;
            }
            

            /*
            计算公式如下：
            距离D=音速V*T/2;(T是上述的脉宽时间，因为是来回时间，所以要除以2才是单程时间)
            D(单位cm)=340m/s*Tus;
            D=34000cm/s*(10^-6)Ts/2;(1us=(10^-6)s)
            D=17000*(10^-6)*T
            D=0.017*T
            D=T/58
            所以下面这个公式/58是这样来的
            */
            float distance = (float) pulse_width_us / 58;
            ESP_LOGI(TAG, "Measured distance: %.2fcm", distance);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

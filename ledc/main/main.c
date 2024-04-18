#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "driver/gpio.h" 
#include <esp_log.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include "esp32/rom/ets_sys.h"
#include "driver/ledc.h"

//定义LED的GPIO口
#define LED_GPIO  GPIO_NUM_27

#define TAG     "LEDC"

//LED闪烁运行任务
void led_run_task(void* param)
{
    bool light_flg = false; //LED亮灯标记
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        if(light_flg)
        {
            gpio_set_level(LED_GPIO,0);
            light_flg = false;
        }
        else
        {
            light_flg = true;
            gpio_set_level(LED_GPIO,1);
        }
    }
}

//LED闪烁初始化
void led_flash_init(void)
{
    gpio_config_t led_gpio_cfg = {
        .pin_bit_mask = (1<<LED_GPIO),          //指定GPIO
        .mode = GPIO_MODE_OUTPUT,               //设置为输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,      //禁止上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  //禁止下拉
        .intr_type = GPIO_INTR_DISABLE,         //禁止中断
    };
    gpio_config(&led_gpio_cfg);

    xTaskCreatePinnedToCore(led_run_task,"led",2048,NULL,3,NULL,1);
}


#define LEDC_TIMER              LEDC_TIMER_0            //定时器0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE     //低速模式
#define LEDC_OUTPUT_IO          (LED_GPIO)              //选择GPIO端口
#define LEDC_CHANNEL            LEDC_CHANNEL_0          //PWM通道
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT       //分辨率
#define LEDC_DUTY               (4095)                  //最大占空比值，这里是2^13-1
#define LEDC_FREQUENCY          (5000)                  //PWM周期

//用于通知渐变完成
static EventGroupHandle_t   s_ledc_ev = NULL;

//关灯完成事件标志
#define LEDC_OFF_EV  (1<<0)

//开灯完成事件标志
#define LEDC_ON_EV   (1<<1)

//渐变完成回调函数
bool ledc_finish_cb(const ledc_cb_param_t *param, void *user_arg)
{
    BaseType_t xHigherPriorityTaskWoken;
    if(param->duty)
    {
        xEventGroupSetBitsFromISR(s_ledc_ev,LEDC_ON_EV,&xHigherPriorityTaskWoken);
    }
    else
    {
        xEventGroupSetBitsFromISR(s_ledc_ev,LEDC_OFF_EV,&xHigherPriorityTaskWoken);
    }
    return xHigherPriorityTaskWoken;
}

//ledc 渐变任务
void IRAM_ATTR ledc_breath_task(void* param)
{
    EventBits_t ev;
    while(1)
    {
        ev = xEventGroupWaitBits(s_ledc_ev,LEDC_ON_EV|LEDC_OFF_EV,pdTRUE,pdFALSE,pdMS_TO_TICKS(5000));
        if(ev)
        {
            //设置LEDC开灯渐变
            if(ev & LEDC_OFF_EV)
            {
                ledc_set_fade_with_time(LEDC_MODE,LEDC_CHANNEL,LEDC_DUTY,2000);
                ledc_fade_start(LEDC_MODE,LEDC_CHANNEL,LEDC_FADE_NO_WAIT);
            }
            else if(ev & LEDC_ON_EV)    //设置LEDC关灯渐变
            {
                ledc_set_fade_with_time(LEDC_MODE,LEDC_CHANNEL,0,2000);
                ledc_fade_start(LEDC_MODE,LEDC_CHANNEL,LEDC_FADE_NO_WAIT);
            }
            //再次设置回调函数
            ledc_cbs_t cbs = {.fade_cb=ledc_finish_cb,};
            ledc_cb_register(LEDC_MODE,LEDC_CHANNEL,&cbs,NULL);
        }
    }
}

//LED呼吸灯初始化
void led_breath_init(void)
{

    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,        //低速模式
        .channel        = LEDC_CHANNEL,     //PWM 通道0-7
        .timer_sel      = LEDC_TIMER,       //关联定时器，也就是上面初始化好的那个定时器
        .intr_type      = LEDC_INTR_DISABLE,//不使能中断
        .gpio_num       = LEDC_OUTPUT_IO,   //设置输出PWM方波的GPIO管脚
        .duty           = 0, // 设置默认占空比为0
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    //开启硬件PWM
    ledc_fade_func_install(0);

    //创建一个事件组，用于通知任务渐变完成
    s_ledc_ev = xEventGroupCreate();

    //配置LEDC渐变
    ledc_set_fade_with_time(LEDC_MODE,LEDC_CHANNEL,LEDC_DUTY,2000);

    //启动渐变
    ledc_fade_start(LEDC_MODE,LEDC_CHANNEL,LEDC_FADE_NO_WAIT);

    //设置渐变完成回调函数
    ledc_cbs_t cbs = {.fade_cb=ledc_finish_cb,};
    ledc_cb_register(LEDC_MODE,LEDC_CHANNEL,&cbs,NULL);

    xTaskCreatePinnedToCore(ledc_breath_task,"ledc",2048,NULL,3,NULL,1);
}


// 主函数
void app_main(void)
{
	//led_flash_init();     //简单led闪烁
    led_breath_init();      //呼吸灯
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <sys/stat.h>
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht11.h"
#include "softap.h"
#include "ws.h"
#include "cJSON.h"

//LED GPIO
#define LED_PIN     GPIO_NUM_27

//DHT11 GPIO
#define DHT11_PIN   GPIO_NUM_25

#define TAG     "main"

//LED状态
static int led_state = 0;
//温度
static float s_fTemp = 0.0f;
//湿度
static int s_iHumidity = 0;

#define INDEX_HTML_PATH "/spiffs/esp.html"
char index_html[4096];

/** 从spiffs中加载html页面到内存
 * @param 无
 * @return 无 
*/
static void initi_web_page_buffer(void)
{
    //定义挂载点
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
        };
    //挂载spiffs
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    memset((void *)index_html, 0, sizeof(index_html));
    //查找文件是否存在
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
        ESP_LOGE(TAG, "esp.html not found");
        return;
    }
    //打开html文件并且读取到内存中
    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(index_html, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
}

/** 接收到WebSocket数据，触发此回调函数
 * @param payload 数据
 * @param len 值
 * @return 无 
*/
void esp_ws_receive(const char* payload,int len)
{
    ESP_LOGI(TAG,"ws receive:%s",payload);
    if(strstr(payload,"toggle"))
    {
        led_state = led_state?0:1;
        gpio_set_level(LED_PIN,led_state);
    }
}

/** WebSocket服务器需要周期发送的数据
 * @param payload 需要发送的数据
 * @param len 数据长度
 * @return 无 
*/
void esp_ws_send(char* send_buf,int *len)
{
    cJSON *js = cJSON_CreateObject();
    if(!js)
    {
        *len = 0;
        return;
    }
    char str_buf[16] = {0};
    //led
    if(led_state)
        snprintf(str_buf,16,"%s","ON");
    else
        snprintf(str_buf,16,"%s","OFF");
    cJSON_AddStringToObject(js,"led",str_buf);

    //温度
    snprintf(str_buf,16,"%.1f",s_fTemp);
    cJSON_AddStringToObject(js,"temp",str_buf);

    //湿度
    snprintf(str_buf,16,"%d",s_iHumidity);
    cJSON_AddStringToObject(js,"humidity",str_buf);

    char * js_value = cJSON_PrintUnformatted(js);
    sprintf(send_buf,"%s",js_value);
    *len = strlen(send_buf);
    ESP_LOGI(TAG,"ws send:%s",send_buf);

    cJSON_free(js_value);
    cJSON_Delete(js);
}


void dht11_task(void* param)
{
    while(1)
    {
        int temp_x10 = 0;
        vTaskDelay(pdMS_TO_TICKS(2500));
        if(DHT11_StartGet(&temp_x10,&s_iHumidity))
        {
            s_fTemp = (float)temp_x10/10.0;
            //ESP_LOGI(TAG,"temp:%.1f,humidity:%d",s_fTemp,s_iHumidity);
        }
        
        
    }
}

void app_main()
{
    /*初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /*初始化LED*/
    gpio_config_t led_gpio_cfg = {
        .pin_bit_mask = (1<<LED_PIN),          //指定GPIO
        .mode = GPIO_MODE_OUTPUT,               //设置为输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,      //禁止上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  //禁止下拉
        .intr_type = GPIO_INTR_DISABLE,         //禁止中断
    };
    gpio_config(&led_gpio_cfg);
    gpio_set_level(LED_PIN,0);
    led_state = 0;

    /*初始化DHT11*/
    DHT11_Init(DHT11_PIN);

    /*初始化SOFTAP*/
    softap_init();

    /*加载html页面*/
    initi_web_page_buffer();

    /*初始化WebSocket*/
    ws_cfg_t    ws;
    ws.html_code = index_html;
    ws.intervel_ms = 2000;
    ws.send_fn = esp_ws_send;
    ws.receive_fn = esp_ws_receive;
    web_monitor_init(&ws);

    ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
    xTaskCreatePinnedToCore(dht11_task,"dht11",4096,NULL,4,NULL,1);
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "simple_wifi_sta.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char* TAG = "main";

#define WIFI_CONNECT_BIT    BIT0
static EventGroupHandle_t s_wifi_ev = NULL;

//心知天气秘钥
#define WEATHER_PRIVATE_KEY "SX3i-wrEj5RcmFPQf"



#define MAX_OUTPUT_BUFFER_LEN   1024
static  char output_buffer[MAX_OUTPUT_BUFFER_LEN] = {0};   //用于接收通过http协议返回的数据
static int output_len = 0;

//当获取到城市的名字后，保存在如下数组
static char s_city_name[128];

#define CLIENT_HTTP_RECEIVE_BIT BIT0
static EventGroupHandle_t   s_client_http_event = NULL; //此事件用于通知http接收完成


static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:    //错误事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:    //连接成功事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:    //发送头事件
            //ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:    //接收头事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:    //接收数据事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            xEventGroupClearBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT);
            if(output_len + evt->data_len < MAX_OUTPUT_BUFFER_LEN)
            {
                memcpy(&output_buffer[output_len], evt->data,evt->data_len);
                output_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:    //会话完成事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            xEventGroupSetBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT);
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:    //断开事件
            //ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            //ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/** 解析返回的天气数据
 * @param json数据
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t parse_weather(char* weather_js)
{
    cJSON *wt_js = cJSON_Parse(weather_js);
    if(!wt_js)
    {
        ESP_LOGI(TAG,"invaild json format");
        return ESP_FAIL;
    }
    cJSON *result_js = cJSON_GetObjectItem(wt_js,"results");
    if(!result_js)
    {
        ESP_LOGI(TAG,"invaild weather result");
        return ESP_FAIL;
    }
    cJSON *result_child_js = result_js->child;
    while(result_child_js)
    {
        cJSON *location_js = cJSON_GetObjectItem(result_child_js,"location");
        cJSON *name_js = NULL;
        if(location_js)
        {
            name_js = cJSON_GetObjectItem(location_js,"name");
        }
        cJSON *now_js = cJSON_GetObjectItem(result_child_js,"now");
        cJSON *temp_js = NULL;
        if(now_js)
        {
            temp_js = cJSON_GetObjectItem(now_js,"temperature");
        }
        if(name_js && temp_js)
        {
            ESP_LOGI(TAG,"city:%s,temperature:%s",cJSON_GetStringValue(name_js),cJSON_GetStringValue(temp_js));
        }
        result_child_js = result_child_js->next;
    }
    return ESP_OK;
}

/** 解析返回的位置数据
 * @param json数据
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t parse_location(char* location_js)
{
    cJSON *lo_js = cJSON_Parse(location_js);
    if(!lo_js)
    {
        ESP_LOGI(TAG,"invaild json format");
        return ESP_FAIL;
    }
    cJSON *city_js = cJSON_GetObjectItem(lo_js,"city");
    if(!city_js)
    {
        ESP_LOGI(TAG,"invaild location result");
        return ESP_FAIL;
    }
    snprintf(s_city_name,sizeof(s_city_name),"%s",cJSON_GetStringValue(city_js));
    return ESP_OK;
}

/** 发起http请求，获取天气数据
 * @param 无
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t weather_http_init(void)
{
    esp_err_t ret_code = ESP_FAIL;
    static char weather_url[256];

    //定义http配置结构体
    esp_http_client_config_t config;
    snprintf(weather_url,sizeof(weather_url),
    "https://api.seniverse.com/v3/weather/now.json?key=%s&location=%s&language=zh-Hans&unit=c",
        WEATHER_PRIVATE_KEY,s_city_name);
    config.url = weather_url;
    config.skip_cert_common_name_check = true;
    config.event_handler = http_client_event_handler;
    //初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);	//初始化http连接
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    //设置发送请求 
    esp_http_client_set_method(http_client, HTTP_METHOD_POST);

    esp_err_t err  = esp_http_client_perform(http_client);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    //获取返回的内容长度
    output_len =  esp_http_client_get_content_length(http_client);
    if(output_len > 0)
    {
        output_len = esp_http_client_read(http_client, output_buffer, MAX_OUTPUT_BUFFER_LEN);
        ret_code = parse_weather(output_buffer);
        output_len = 0;
    }
    else
    {
        //chunked块，需要从https event_handle获取数据
        EventBits_t ev = xEventGroupWaitBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT,pdTRUE,pdFALSE,10*1000/portTICK_PERIOD_MS);
        if(ev & CLIENT_HTTP_RECEIVE_BIT)
        {
            ret_code = parse_weather(output_buffer);
            output_len = 0;
        }
    }
    return ret_code;
}

/** 发起http请求，获取当前城市
 * @param 无
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t location_http_init(void)
{
    esp_err_t ret_code = ESP_OK;

    //定义http配置结构体
    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/?lang=en",
        .skip_cert_common_name_check = true,
        .event_handler = http_client_event_handler,
    };  
    //初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);	//初始化http连接
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    //设置发送请求 
    esp_http_client_set_method(http_client, HTTP_METHOD_POST);

    esp_err_t err  = esp_http_client_perform(http_client);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    output_len =  esp_http_client_get_content_length(http_client);
    if(output_len > 0)
    {
        output_len = esp_http_client_read(http_client, output_buffer, MAX_OUTPUT_BUFFER_LEN);
        ret_code = parse_location(output_buffer);
        output_len = 0;
    }
    else
    {
        //chunked块，需要从https event_handle获取数据
        EventBits_t ev = xEventGroupWaitBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT,pdTRUE,pdFALSE,10*1000/portTICK_PERIOD_MS);
        if(ev & CLIENT_HTTP_RECEIVE_BIT)
        {
            ret_code = parse_location(output_buffer);
            output_len = 0;
        }
    }
    return ret_code;
}

/** wifi事件通知
 * @param 无
 * @return 无
*/
void wifi_event_handler(WIFI_EV_e ev)
{
    if(ev == WIFI_CONNECTED)
    {
        xEventGroupSetBits(s_wifi_ev,WIFI_CONNECT_BIT);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        //NVS出现错误，执行擦除
        ESP_ERROR_CHECK(nvs_flash_erase());
        //重新尝试初始化
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    s_wifi_ev = xEventGroupCreate();
    EventBits_t ev = 0;
    s_client_http_event = xEventGroupCreate();
    //初始化WIFI，传入回调函数，用于通知连接成功事件
    wifi_sta_init(wifi_event_handler);

    //一直监听WIFI连接事件，直到WiFi连接成功后，才发起天气请求
    ev = xEventGroupWaitBits(s_wifi_ev,WIFI_CONNECT_BIT,pdTRUE,pdFALSE,portMAX_DELAY);
    if(ev & WIFI_CONNECT_BIT)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        //首先发起位置获取请求，成功后才去获取天气信息
        if(location_http_init() == ESP_OK)
            weather_http_init();
    }
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return;
}

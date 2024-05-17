#include "aliot_register.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include <time.h>
#include "mbedtls/md5.h" 
#include <stdint.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "mbedtls/md5.h"
#include "mbedtls/md.h"
#include "aliot.h"

#define TAG     "aliot_reg"

//连接事件
#define     EV_REGISTER_SUC     (1<<0)
static EventGroupHandle_t     s_register_ev;

//mqtt连接客户端
static esp_mqtt_client_handle_t s_register_client = NULL;

//注册结果
static char s_cregister_result = 0;

/**
 * mqtt事件处理函数
 * @param event 事件参数
 * @return 无
 */
static void register_mqtt_event_handler(void* event_handler_arg,
                                        esp_event_base_t event_base,
                                        int32_t event_id,
                                        void* event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    // your_context_t *context = event->context;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:  //连接成功
            ESP_LOGI(TAG, "devic mqtt connected");
            break;
        case MQTT_EVENT_DISCONNECTED:   //连接断开
            ESP_LOGI(TAG, "device mqtt disconnected");
            break;

        case MQTT_EVENT_SUBSCRIBED:     //收到订阅消息ACK
            ESP_LOGI(TAG, "device mqtt subscribed ack, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:   //收到解订阅消息ACK

            break;
        case MQTT_EVENT_PUBLISHED:      //收到发布消息ACK
            //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            ESP_LOGI(TAG, "device mqtt publish ack, msg_id=%d", event->msg_id);

            break;
        case MQTT_EVENT_DATA:
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            if(strstr(event->topic,"/ext/register"))
            {
                cJSON *js_reg = cJSON_Parse(event->data);
                if(js_reg)
                {
                    cJSON *js_secret = cJSON_GetObjectItem(js_reg,"deviceSecret");
                    if(js_secret && cJSON_IsString(js_secret))
                    {
                        aliot_set_devicesecret(cJSON_GetStringValue(js_secret));
                        s_cregister_result = 1;
                        xEventGroupSetBits(s_register_ev,EV_REGISTER_SUC);
                        ESP_LOGI(TAG,"aliot register success!!");
                    }
                    cJSON_Delete(js_reg);
                }
                
                /*
                {
                    "productKey" : "***",
                    "deviceName" : "***",
                    "deviceSecret" : "***"
                    }
                */

            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

            break;
        default:
            break;
    }
}

/**
 * 获取动态注册mqtt配置
 * @param 无
 * @return mqtt动态注册连接参数
 */
static const esp_mqtt_client_config_t * aliot_mqtt_get_register_client_cfg(void)
{
    static esp_mqtt_client_config_t s_mqtt_client = {0};
    s_mqtt_client.broker.address.uri = "mqtts://"ALIOT_PRODUCT_KEY".iot-as-mqtt."ALIOT_REGION_ID".aliyuncs.com";
    s_mqtt_client.broker.address.port = 1883;
    //s_mqtt_client.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    //随机数
    char random[16];
    int r = rand();
    snprintf(random,16,"%d",r);

    //client id
    static char client_id[128];
    snprintf(client_id,sizeof(client_id),"%s|securemode=2,authType=register,random=%s,signmethod=hmacmd5|",aliot_get_clientid(),random);
    s_mqtt_client.credentials.client_id = client_id;

    //用户名
    static char username[128];
    snprintf(username,sizeof(username),"%s&%s",aliot_get_devicename(),ALIOT_PRODUCT_KEY);
    s_mqtt_client.credentials.username = username;

    //密码
    static char str_password[33];
    char sign_content[256];
    unsigned char password[16];
    snprintf(sign_content,256,"deviceName%sproductKey%srandom%s",aliot_get_devicename(),ALIOT_PRODUCT_KEY,random);
    calc_hmd5(ALIOT_PRODUCT_SECRET,sign_content,password);
    core_hex2str(password,16,str_password,0);
    s_mqtt_client.credentials.authentication.password = str_password;

    ESP_LOGI(TAG,"aliot register->clientId:%s,username:%s,password:%s",client_id,username,str_password);

    //CA根证书
    s_mqtt_client.broker.verification.certificate = g_aliot_ca;
    //s_mqtt_client.broker.verification.certificate_len = strlen(g_aliot_ca);

    return &s_mqtt_client;
}

/**
 * 发起动态注册，获取设备秘钥
 * @param 无
 * @return 无
 */
static void aliot_register_task(void* arg)
{
    EventBits_t ev = 0xFF;
    
    const esp_mqtt_client_config_t *mqtt_cfg = aliot_mqtt_get_register_client_cfg();
    s_register_client = esp_mqtt_client_init(mqtt_cfg);
    esp_mqtt_client_register_event(s_register_client, ESP_EVENT_ANY_ID, register_mqtt_event_handler, s_register_client);
    esp_mqtt_client_start(s_register_client);
    while(1)
    {
        ev = xEventGroupWaitBits(s_register_ev,EV_REGISTER_SUC,pdTRUE,pdFALSE,pdMS_TO_TICKS(60*1000));
        if(ev & EV_REGISTER_SUC)
        {
            esp_mqtt_client_disconnect(s_register_client);
            esp_mqtt_client_stop(s_register_client);
            break;
        }
    }
    vTaskDelete(NULL);
}

/**
 * 发起动态注册，获取设备秘钥
 * @param 无
 * @return 无
 */
void aliot_start_register(void)
{
    if(s_register_ev == NULL)
        s_register_ev = xEventGroupCreate();
    xTaskCreatePinnedToCore(aliot_register_task, "aliot_reg", 4096, NULL,2, NULL, tskNO_AFFINITY);
}

/**
 * 查询注册结果
 * @param 无
 * @return 成功非0,失败0
 */
char aliot_register_result(void)
{
    return s_cregister_result;
}

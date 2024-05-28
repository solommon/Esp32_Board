#include "onenet_mqtt.h"
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
#include "onenet.h"
#include "onenet_dm.h"

#define TAG     "onenet_mqtt"

//连接成功标志位
static bool s_onenet_connected_flg = false;

//mqtt连接客户端
static esp_mqtt_client_handle_t s_onenet_client = NULL;

//属性下发回调函数
static fp_property_cb s_fproperty_set = NULL;


static void onenet_property_ack(int code,const char* message);

/**
 * mqtt连接事件处理函数
 * @param event 事件参数
 * @return 无
 */
static void onenet_mqtt_event_handler(void* event_handler_arg,
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
            s_onenet_connected_flg = true;
            break;
        case MQTT_EVENT_DISCONNECTED:   //连接断开
            ESP_LOGI(TAG, "device mqtt disconnected");
            s_onenet_connected_flg = false;
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
            if(strstr(event->topic,"/property/set"))
            {
                cJSON *property_js = cJSON_Parse(event->data);
                cJSON *params_js = cJSON_GetObjectItem(property_js,"params");
                if(params_js)
                {
                    cJSON *name_js = params_js->child;
                    while(name_js)
                    {
                        if(s_fproperty_set)
                        {
                            onenet_value value;
                            if(cJSON_IsNumber(name_js))
                            {
                                value.f_value = cJSON_GetNumberValue(name_js);
                                value.i_value = cJSON_GetNumberValue(name_js);
                            }
                            if(cJSON_IsString(name_js))
                            {
                                value.s_value = cJSON_GetStringValue(name_js);
                            }
                            s_fproperty_set(name_js->string,value);
                        }
                        name_js = name_js->next;
                    }
                }
                onenet_property_ack(200,"success");
            
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
 * 返回属性设置确认
 * @param code 错误码
 * @param message 信息
 * @return mqtt连接参数
 */
static void onenet_property_ack(int code,const char* message)
{
    char topic[128];
    ONENET_DM_DES* dm_des = onenet_malloc_dm(ONENET_DM_SET_ACK);
    onenet_set_property_ack(dm_des,code,message);
    onenet_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/set_reply",ONENET_PRODUCT_KEY,onenet_get_devicename());
    esp_mqtt_client_publish(s_onenet_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0); 
    onenet_free_dm(dm_des);
}

/**
 * 获取连接参数
 * @param 无
 * @return mqtt连接参数
 */
static const esp_mqtt_client_config_t * onenet_mqtt_get_connect_client_cfg(void)
{
    static esp_mqtt_client_config_t s_mqtt_client = {0};
    //s_mqtt_client.broker.address.uri = "mqtts://mqttstls.heclouds.com:8883";
    //s_mqtt_client.broker.address.port = 8883;
    s_mqtt_client.broker.address.uri = "mqtt://mqtts.heclouds.com";
    s_mqtt_client.broker.address.port = 1883;


    //client id
    s_mqtt_client.credentials.client_id = onenet_get_devicename();

    //用户名
    s_mqtt_client.credentials.username = ONENET_PRODUCT_KEY;

    //密码
    static uint8_t token[256];
    dev_token_generate(token, SIG_METHOD_SHA256, TM_EXPIRE_TIME, ONENET_PRODUCT_KEY, onenet_get_devicename(), ONENET_ACCESS_KEY);
    s_mqtt_client.credentials.authentication.password = token;

    ESP_LOGI(TAG,"onenet connect->clientId:%s,username:%s,password:%s",s_mqtt_client.credentials.client_id,
    s_mqtt_client.credentials.username,s_mqtt_client.credentials.authentication.password);
    
    //CA根证书
    //s_mqtt_client.broker.verification.certificate = g_onenet_ca;

    return &s_mqtt_client;
}

/**
 * mqtt任务
 * @param 无
 * @return 无
 */
static void onenet_mqtt_run(void *args) 
{
    EventBits_t ev = 0xFF;

    const esp_mqtt_client_config_t *mqtt_cfg = onenet_mqtt_get_connect_client_cfg();
    s_onenet_client = esp_mqtt_client_init(mqtt_cfg);
    esp_mqtt_client_register_event(s_onenet_client, ESP_EVENT_ANY_ID, onenet_mqtt_event_handler, s_onenet_client);
    esp_mqtt_client_start(s_onenet_client);
    while(1)
    {
        
        vTaskDelay(60*1000/portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


/**
 * 启动onenet连接（启动前请确保wifi连接正常，产品id和产品秘钥已正确修改）
 * @param 无
 * @return 无
 */
void onenet_start(void)
{
    xTaskCreatePinnedToCore(onenet_mqtt_run, "onenet_run", 4096, NULL,3, NULL, tskNO_AFFINITY);
}

/**
 * 设置属性下发控制回调函数
 * @param f 回调函数 
 * @return 无
 */
void onenet_set_property_cb(fp_property_cb f)
{
    s_fproperty_set = f;
}

/**
 * 上报单个属性值（浮点）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_double(const char* name,double value)
{
    if (!s_onenet_connected_flg)
        return;
    char topic[128];
    ONENET_DM_DES* dm_des = onenet_malloc_dm(ONENET_DM_POST);
    onenet_set_dm_double(dm_des,name,value);
    onenet_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post",
        ONENET_PRODUCT_KEY,onenet_get_devicename());
    esp_mqtt_client_publish(s_onenet_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0);
    onenet_free_dm(dm_des);
}

/**
 * 上报单个属性值（整形）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_int(const char* name,int value)
{
    if (!s_onenet_connected_flg)
        return;
    char topic[128];
    ONENET_DM_DES* dm_des = onenet_malloc_dm(ONENET_DM_POST);
    onenet_set_dm_int(dm_des,name,value);
    onenet_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post",
        ONENET_PRODUCT_KEY,onenet_get_devicename());
    esp_mqtt_client_publish(s_onenet_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0);
    onenet_free_dm(dm_des);
}

/**
 * 上报单个属性值（字符串）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_str(const char* name,const char* value)
{
    if (!s_onenet_connected_flg)
        return;
    char topic[128];
    ONENET_DM_DES* dm_des = onenet_malloc_dm(ONENET_DM_POST);
    onenet_set_dm_str(dm_des,name,value);
    onenet_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post",
        ONENET_PRODUCT_KEY,onenet_get_devicename());
    esp_mqtt_client_publish(s_onenet_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0);
    onenet_free_dm(dm_des);
}

/**
 * 上报整个json
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_data(const char* js)
{
    if (!s_onenet_connected_flg)
        return;
    char topic[128];
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post",
        ONENET_PRODUCT_KEY,onenet_get_devicename());
    esp_mqtt_client_publish(s_onenet_client,topic,js,strlen(js),1,0);
}


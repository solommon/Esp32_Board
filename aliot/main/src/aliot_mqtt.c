#include "aliot_mqtt.h"
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
#include "aliot_register.h"
#include "aliot_dm.h"

#define TAG     "aliot_mqtt"

//连接成功标志位
static bool s_aliot_connected_flg = false;

//mqtt连接客户端
static esp_mqtt_client_handle_t s_aliot_client = NULL;

//属性下发回调函数
static fp_property_cb s_fproperty_set = NULL;


static void aliot_property_ack(int code,const char* message);

/**
 * mqtt连接事件处理函数
 * @param event 事件参数
 * @return 无
 */
static void aliot_mqtt_event_handler(void* event_handler_arg,
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
            s_aliot_connected_flg = true;
            break;
        case MQTT_EVENT_DISCONNECTED:   //连接断开
            ESP_LOGI(TAG, "device mqtt disconnected");
            s_aliot_connected_flg = false;
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
                            aliot_value value;
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
                aliot_property_ack(200,"success");
            
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
static void aliot_property_ack(int code,const char* message)
{
    char topic[128];
    ALIOT_DM_DES* dm_des = aliot_malloc_dm(ALIOT_DM_SET_ACK);
    aliot_set_property_ack(dm_des,code,message);
    aliot_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"/sys/%s/%s/thing/service/property/set_reply",ALIOT_PRODUCT_KEY,aliot_get_devicename());
    esp_mqtt_client_publish(s_aliot_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0); 
    aliot_free_dm(dm_des);
}

/**
 * 获取连接参数
 * @param 无
 * @return mqtt连接参数
 */
static const esp_mqtt_client_config_t * aliot_mqtt_get_connect_client_cfg(void)
{
    static esp_mqtt_client_config_t s_mqtt_client = {0};
    s_mqtt_client.broker.address.uri = "mqtts://"ALIOT_PRODUCT_KEY".iot-as-mqtt."ALIOT_REGION_ID".aliyuncs.com";
    s_mqtt_client.broker.address.port = 1883;

    //时间戳
    char str_t[16];
    time_t t = time(0);
    snprintf(str_t,16,"%lld000",t);

    //client id
    static char client_id[128];
    snprintf(client_id,sizeof(client_id),"%s|securemode=2,signmethod=hmacmd5,timestamp=%s|",aliot_get_clientid(),str_t);
    s_mqtt_client.credentials.client_id = client_id;

    //用户名
    static char username[128];
    snprintf(username,sizeof(username),"%s&%s",aliot_get_devicename(),ALIOT_PRODUCT_KEY);
    s_mqtt_client.credentials.username = username;

    //密码
    static char str_password[33];
    char sign_content[256];
    unsigned char password[16];
    snprintf(sign_content,256,"clientId%sdeviceName%sproductKey%stimestamp%s",
    aliot_get_clientid(),aliot_get_devicename(),ALIOT_PRODUCT_KEY,str_t);
    calc_hmd5(aliot_get_devicesecret(),sign_content,password);
    core_hex2str(password,16,str_password,0);
    s_mqtt_client.credentials.authentication.password = str_password;

    ESP_LOGI(TAG,"aliot connect->clientId:%s,username:%s,password:%s",client_id,username,str_password);
    
    //CA根证书
    s_mqtt_client.broker.verification.certificate = g_aliot_ca;
    //s_mqtt_client.broker.verification.certificate_len = strlen(g_aliot_ca);

    return &s_mqtt_client;
}

/**
 * mqtt任务
 * @param 无
 * @return 无
 */
static void aliot_mqtt_run(void *args) 
{
    EventBits_t ev = 0xFF;

    //首先看下设备秘钥是否已经获取
    char* devSecret = aliot_get_devicesecret();
    if(devSecret[0] == 0)   //秘钥不合法，发起注册
    {
        aliot_start_register();
        while(!aliot_register_result()) //直到查询到正确结果后才中止循环
        {
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        ESP_LOGI(TAG,"get devicesecret success!!");
    }
    else
    {
        ESP_LOGI(TAG,"already get devicesecret");
    }
    
    const esp_mqtt_client_config_t *mqtt_cfg = aliot_mqtt_get_connect_client_cfg();
    s_aliot_client = esp_mqtt_client_init(mqtt_cfg);
    esp_mqtt_client_register_event(s_aliot_client, ESP_EVENT_ANY_ID, aliot_mqtt_event_handler, s_aliot_client);
    esp_mqtt_client_start(s_aliot_client);
    while(1)
    {
        
        vTaskDelay(60*1000/portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


/**
 * 启动aliot连接（启动前请确保wifi连接正常，产品id和产品秘钥已正确修改）
 * @param 无
 * @return 无
 */
void aliot_start(void)
{
    xTaskCreatePinnedToCore(aliot_mqtt_run, "aliot_run", 4096, NULL,3, NULL, tskNO_AFFINITY);
}

/**
 * 设置属性下发控制回调函数
 * @param f 回调函数 
 * @return 无
 */
void aliot_set_property_cb(fp_property_cb f)
{
    s_fproperty_set = f;
}

/**
 * 上报单个属性值（浮点）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void aliot_post_property_double(const char* name,double value)
{
    if (!s_aliot_connected_flg)
        return;
    char topic[128];
    ALIOT_DM_DES* dm_des = aliot_malloc_dm(ALIOT_DM_POST);
    aliot_set_dm_double(dm_des,name,value);
    aliot_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"/sys/%s/%s/thing/event/property/post",
        ALIOT_PRODUCT_KEY,aliot_get_devicename());
    esp_mqtt_client_publish(s_aliot_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0);
    aliot_free_dm(dm_des);
}

/**
 * 上报单个属性值（整形）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void aliot_post_property_int(const char* name,int value)
{
    if (!s_aliot_connected_flg)
        return;
    char topic[128];
    ALIOT_DM_DES* dm_des = aliot_malloc_dm(ALIOT_DM_POST);
    aliot_set_dm_int(dm_des,name,value);
    aliot_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"/sys/%s/%s/thing/event/property/post",
        ALIOT_PRODUCT_KEY,aliot_get_devicename());
    esp_mqtt_client_publish(s_aliot_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0);
    aliot_free_dm(dm_des);
}

/**
 * 上报单个属性值（字符串）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void aliot_post_property_str(const char* name,const char* value)
{
    if (!s_aliot_connected_flg)
        return;
    char topic[128];
    ALIOT_DM_DES* dm_des = aliot_malloc_dm(ALIOT_DM_POST);
    aliot_set_dm_str(dm_des,name,value);
    aliot_dm_serialize(dm_des);
    snprintf(topic,sizeof(topic),"/sys/%s/%s/thing/event/property/post",
        ALIOT_PRODUCT_KEY,aliot_get_devicename());
    esp_mqtt_client_publish(s_aliot_client,topic,dm_des->dm_js_str,dm_des->data_len,1,0);
    aliot_free_dm(dm_des);
}

/**
 * 上报整个json
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void aliot_post_property_data(const char* js)
{
    if (!s_aliot_connected_flg)
        return;
    char topic[128];
    snprintf(topic,sizeof(topic),"/sys/%s/%s/thing/event/property/post",
        ALIOT_PRODUCT_KEY,aliot_get_devicename());
    esp_mqtt_client_publish(s_aliot_client,topic,js,strlen(js),1,0);
}


#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"

//一个事件组，用于表示
static EventGroupHandle_t s_wifi_event_group;

//连接成功事件
static const int CONNECTED_BIT = BIT0;

//smartconfig完成事件
static const int ESPTOUCH_DONE_BIT = BIT1;

static const char *TAG = "smartconfig_example";

#define NVS_WIFI_NAMESPACE_NAME         "DEV_WIFI"
#define NVS_SSID_KEY                    "ssid"
#define NVS_PASSWORD_KEY                "password"

//缓存一份ssid
static char s_ssid_value[33] = {0};

//缓存一份password
static char s_password_value[65] = {0};

//用一个标志来表示是否处于smartconfig中
static bool s_is_smartconfig = false;

/** 从NVS中读取SSID
 * @param ssid 读到的ssid
 * @param maxlen 外部存储ssid数组的最大值
 * @return 读取到的字节数
*/
static size_t read_nvs_ssid(char* ssid,int maxlen)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret_val = ESP_FAIL;
    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &nvs_handle));
    ret_val = nvs_get_str(nvs_handle, NVS_SSID_KEY, NULL, &required_size);
    if(ret_val == ESP_OK && required_size <= maxlen)
    {
        nvs_get_str(nvs_handle,NVS_SSID_KEY,ssid,&required_size);
    }
    else
        required_size = 0;
    nvs_close(nvs_handle);
    return required_size;
}

/** 写入SSID到NVS中
 * @param ssid 需写入的ssid
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t write_nvs_ssid(char* ssid)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &nvs_handle));
    
    ret = nvs_set_str(nvs_handle, NVS_SSID_KEY, ssid);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

/** 从NVS中读取PASSWORD
 * @param ssid 读到的password
 * @param maxlen 外部存储password数组的最大值
 * @return 读取到的字节数
*/
static size_t read_nvs_password(char* pwd,int maxlen)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret_val = ESP_FAIL;
    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &nvs_handle));
    ret_val = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    if(ret_val == ESP_OK && required_size <= maxlen)
    {
        nvs_get_str(nvs_handle,NVS_SSID_KEY,pwd,&required_size);
    }
    else 
        required_size = 0;
    nvs_close(nvs_handle);
    return required_size;
}

/** 写入PASSWORD到NVS中
 * @param pwd 需写入的password
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t write_nvs_password(char* pwd)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &nvs_handle));
    ret = nvs_set_str(nvs_handle, NVS_PASSWORD_KEY, pwd);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}


static void smartconfig_example_task(void * parm);

/** 各种网络事件的回调函数
 * @param arg 自定义参数
 * @param event_base 事件类型
 * @param event_id 事件标识ID，不同的事件类型都有不同的实际标识ID
 * @param event_data 事件携带的数据
*/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        //STA初始化完毕后，启动smartconfig任务
        //xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        if(s_ssid_value[0] != 0)
            esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        //WIFI断开连接后，再次发起连接
        if(!s_is_smartconfig)
            esp_wifi_connect();
        //清除连接标志位
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //获取到IP，置位连接事件标志位
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        //smartconfig 扫描完成
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        //smartconfig 找到对应的通道
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        //smartconfig 获取到SSID和密码
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        //从event_data中提取SSID和密码
        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        snprintf(s_ssid_value,33,"%s",(char*)ssid);
        snprintf(s_password_value,65,"%s",(char*)password);
        //重新连接WIFI
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        //smartconfig 已发起回应
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

    //从NVS中读出SSID
    read_nvs_ssid(s_ssid_value,32);

    //从NVS中读取PASSWORD
    read_nvs_password(s_password_value,64);

    if(s_ssid_value[0] != 0)    //通过SSID第一个字节是否是0，判断是否读取成功，然后设置wifi_config_t
    {
        wifi_config_t wifi_config = 
        {
            .sta = 
            { 
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = 
                {
                    .capable = true,
                    .required = false
                },
            },
        };
        snprintf((char*)wifi_config.sta.ssid,32,"%s",s_ssid_value);
        snprintf((char*)wifi_config.sta.password,64,"%s",s_password_value);
    }

    ESP_ERROR_CHECK( esp_wifi_start() );
}

/** 启动smartconfig
 * @param 无
 * @return 无
*/
void smartconfig_start(void)
{
    if(!s_is_smartconfig)
    {
        s_is_smartconfig = true;
        esp_wifi_disconnect();
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    }
}

static void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {    //收到smartconfig配网完成通知
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();         //停止smartconfig配网
            write_nvs_ssid(s_ssid_value);   //将ssid写入NVS
            write_nvs_password(s_password_value);   //将password写入NVS
            s_is_smartconfig = false;       
            //esp_wifi_connect();             //重新发起wifi连接
            vTaskDelete(NULL);              //退出任务
        }
    }
}

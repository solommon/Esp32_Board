#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include <time.h>
#include <sys/time.h>
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "WIFI"

#define DEFAULT_SSID        "test1234"
#define DEFAULT_PASSWORD    "12345678"

#define NVS_WIFI_NAMESPACE_NAME          "DEV_WIFI"

#define WIFI_SSID_MAX_NUM       64
#define WIFI_PASSWORD_MAX_NUM   64

#define NVS_SSID_KEY               "ssid"
#define NVS_PASSWORD_KEY           "password"

static char wifi_ssid[WIFI_SSID_MAX_NUM];
static char wifi_password[WIFI_PASSWORD_MAX_NUM];

#define WIFI_CONNECTED_BIT (1<<0)
#define WIFI_FAIL_BIT      (1<<1)
static EventGroupHandle_t s_wifi_event_group;

static wifi_manager_callback  s_wifi_cb = NULL;

static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{   
    if(event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:  //WIFI就绪
            if(s_wifi_cb)
                s_wifi_cb(WIFI_EV_READY,0);
            //esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            //连接AP成功
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            //连接AP失败
            if(s_wifi_cb)
                s_wifi_cb(WIFI_EV_DISCONNECTED,0);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            break;
        case WIFI_EVENT_SCAN_DONE:
            //扫描结束
            
            break;
        default:
            break;
        }
    }
    if(event_base == IP_EVENT)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                //获取IP成功
                ESP_LOGI(TAG,"get ip success");
                xEventGroupSetBits(s_wifi_event_group,WIFI_CONNECTED_BIT);
                break;
            case IP_EVENT_STA_LOST_IP:
                //失联
                if(s_wifi_cb)
                    s_wifi_cb(WIFI_EV_DISCONNECTED,0);
                break;
            default:break;
        }
    }
}

//校时成功后的回调
static void time_sync_notification_cb(struct timeval *tv)
{
    settimeofday(tv, NULL);
    if(s_wifi_cb)
        s_wifi_cb(WIFI_EV_SNTP,0);
}

//时间同步
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "time.asia.apple.com");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

/**
 * 初始化参数区
 * @param key       key值
 * @param buf  读取的内容
 * @param max_size  buf最大长度
 * @param read_len  读取到的长度
 * @param default_value 默认值
 * @return 长度
 */
static esp_err_t  dev_wifi_nvs_init(const char* key,uint8_t *buf,uint16_t max_size,const char* default_value)
{
    esp_err_t ret_val = ESP_FAIL;
    size_t required_size = 0;
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE,&handle));
    ret_val = nvs_get_blob(handle, key, NULL, &required_size);
    if(ret_val != ESP_OK)
    {
        size_t cpy_size = strlen(default_value);
        nvs_set_blob(handle,key,default_value,cpy_size);
        ESP_LOGI(TAG,"Not find key %s in NVS,write default",key);
        nvs_commit(handle);
        if(cpy_size > max_size)
            cpy_size = max_size;
        memcpy(buf,default_value,cpy_size);
    }
    else
    {
        uint8_t *read_buf = malloc(required_size);
        if(read_buf != NULL)
        {
            ret_val = nvs_get_blob(handle, key, read_buf, &required_size);
            if(ret_val == ESP_OK)
            {
                size_t cpy_size = required_size>max_size-1?max_size-1:required_size;
                memcpy(buf,read_buf,cpy_size);
                buf[cpy_size] = 0;
                ESP_LOGI(TAG,"Get Key %s success,value:%s",key,(char*)buf);
            }
            free(read_buf);
        }
        
    }
    nvs_close(handle);
    return ret_val;
}


//处理WIFI任务
static void wifi_manager_task(void* param)
{
    dev_wifi_nvs_init(NVS_SSID_KEY,wifi_ssid,sizeof(wifi_ssid),DEFAULT_SSID);
    dev_wifi_nvs_init(NVS_PASSWORD_KEY,wifi_password,sizeof(wifi_password),DEFAULT_PASSWORD);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    //初始化WIFI
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    //注册事件
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&event_handler,NULL));

    //WIFI配置
    wifi_config_t wifi_config = 
    { 
        .sta = 
        { 
            //.ssid = DEFAULT_WIFI_SSID,
            //.password = DEFAULT_WIFI_PASSWORD,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            
            .pmf_cfg = 
            {
                .capable = true,
                .required = false
            },
        },
    };
    
    memcpy(wifi_config.sta.ssid,wifi_ssid,strlen(wifi_ssid));
    wifi_config.sta.ssid[strlen(wifi_ssid)] = 0;
    memcpy(wifi_config.sta.password,wifi_password,strlen(wifi_password));
    wifi_config.sta.password[strlen(wifi_password)] = 0;
    
    //启动WIFI
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    setenv("TZ", "CST-8", 1);
    tzset();
    static char is_sntp_flg = 0;
    vTaskDelay(pdMS_TO_TICKS(1000));
    while(1)
    {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(5000));
        if(bits & WIFI_FAIL_BIT)
        {
            esp_wifi_connect(); //重连
        }
        if(bits & WIFI_CONNECTED_BIT)
        {
            if(!is_sntp_flg)
            {
                initialize_sntp();
                is_sntp_flg = 1;
            }
        }
    }
    vTaskDelete(NULL);
}

/*
 * 初始化
 */
int wifi_manager_init(wifi_manager_callback f)
{
    s_wifi_event_group = xEventGroupCreate();
    xTaskCreatePinnedToCore(wifi_manager_task, "wifi_manager", 4096, NULL, 1, NULL, tskNO_AFFINITY);
    s_wifi_cb = f;
    return 1;
}

//启动WIFI连接
int wifi_manager_start(void)
{
    xEventGroupSetBits(s_wifi_event_group,WIFI_FAIL_BIT);
    //esp_wifi_connect();
    //wifi_manager_scan();

    return 1;
}

//设置ssid和密码
void wifi_manager_set_ssid(char* ssid,char *password)
{
    nvs_handle_t nvs_handle;
    bool write_ssid = false;
    bool write_password = false;
    if(strcmp(ssid,wifi_ssid) !=0 )
        write_ssid = true;

    if(strcmp(password,wifi_password) !=0)
        write_password = true;

    if(write_ssid || write_password)
    {
        ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &nvs_handle));
        if(write_ssid)
        {
            nvs_set_blob(nvs_handle, NVS_SSID_KEY, ssid, strlen(ssid));
            snprintf(wifi_ssid,WIFI_SSID_MAX_NUM,"%s",ssid);
        }
        if(write_password)
        {
            nvs_set_blob(nvs_handle, NVS_PASSWORD_KEY, password, strlen(password));
            snprintf(wifi_password,WIFI_PASSWORD_MAX_NUM,"%s",password);
        }

        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);

        //重新配置WIFI ssid和passwd
        wifi_config_t wifi_config = 
        { 
            .sta = 
            { 
                //.ssid = DEFAULT_WIFI_SSID,
                //.password = DEFAULT_WIFI_PASSWORD,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                
                .pmf_cfg = 
                {
                    .capable = true,
                    .required = false
                },
            },
        };
    
        memcpy(wifi_config.sta.ssid,wifi_ssid,strlen(wifi_ssid));
        wifi_config.sta.ssid[strlen(wifi_ssid)] = 0;
        memcpy(wifi_config.sta.password,wifi_password,strlen(wifi_password));
        wifi_config.sta.password[strlen(wifi_password)] = 0;
        
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    }
}

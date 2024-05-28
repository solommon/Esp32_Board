#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"


#define SOFTAP_ESP_WIFI_AP_SSID            "esp-ap"         //热点名称
#define SOFTAP_ESP_WIFI_AP_PASSWD          "12345678"       //热点密码
#define SOFTAP_ESP_WIFI_CHANNEL            5                //WIFI信道
#define SOFTAP_MAX_STA_CONN                3                //最多连接个数


static const char *TAG_AP = "WiFi SoftAP";

/** wifi事件回调函数
 * @param arg 用户自定义参数
 * @param event_base 事件类型
 * @param event_id 事件ID
 * @param event_data 事件携带的数据
 * @return 无
*/
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        //有设备连接了热点，把它的MAC打印出来
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        //有设备断开了热点
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        //STA启动事件，在本例程中无用
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //STA模式，获取到IP地址，在本例程中无用
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        
    }
}

/** 初始化AP模式
 * @param 无
 * @return AP网卡设备
*/
esp_netif_t *wifi_init_softap(void)
{
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = SOFTAP_ESP_WIFI_AP_SSID,                //热点名称
            .ssid_len = strlen(SOFTAP_ESP_WIFI_AP_SSID),    //长度
            .channel = SOFTAP_ESP_WIFI_CHANNEL,             //信道
            .password = SOFTAP_ESP_WIFI_AP_PASSWD,          //密码
            .max_connection = SOFTAP_MAX_STA_CONN,          //最大连接数
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,             //加密模式
            .pmf_cfg = {
                .required = false,                          //不需要只支持pmf的设备连接
            },
        },
    };

    if (strlen(SOFTAP_ESP_WIFI_AP_PASSWD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    //设置AP模式配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             SOFTAP_ESP_WIFI_AP_SSID, SOFTAP_ESP_WIFI_AP_PASSWD, SOFTAP_ESP_WIFI_CHANNEL);

    return esp_netif_ap;
}

/** 初始化softap
 * @param 无
 * @return 无
*/
void softap_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());      //初始化网卡设备
    ESP_ERROR_CHECK(esp_event_loop_create_default());   //创建事件循环

    //初始化NVS
    //在app_main中已进行初始化

    //注册WIFI事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    NULL,
                    NULL));

    //初始化WIFI
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //设置工作模式为AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    //初始化AP
    ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
    esp_netif_t *esp_netif_ap = wifi_init_softap();

    //设置IP地址
    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 192,168,100,1);
	IP4_ADDR(&ipInfo.gw, 192,168,100,1);
	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	esp_netif_dhcps_stop(esp_netif_ap);
	esp_netif_set_ip_info(esp_netif_ap, &ipInfo);
	esp_netif_dhcps_start(esp_netif_ap);
    //dns_local_addhost


    //启动WIFI工作
    ESP_ERROR_CHECK(esp_wifi_start() );

    //启用NAPT
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK) {
        ESP_LOGE(TAG_AP, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }
}
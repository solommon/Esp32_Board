#include "aliot.h"
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
#include "nvs_flash.h"

#define TAG     "aliot"

#define ALIOT_NVS_NAMESPACE     "aliot"

#define ALIOT_DEVICESECRET_ITEM     "devSecret"

//设备秘钥
static char s_device_secret[128] = { 0 };

/**
 * 获取设备名称
 * @param 无
 * @return 设备名称
 */
char* aliot_get_devicename(void)
{
    static char aliot_mqtt_devicename[64] = {0};
    if(aliot_mqtt_devicename[0] == 0)
    {
        esp_err_t esp_code = ESP_OK;
        uint8_t eth_mac[6];
        esp_code = esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
        if(esp_code != ESP_OK)
            memset(eth_mac,0,sizeof(eth_mac));
        snprintf(aliot_mqtt_devicename,64,"%s%02X%02X%02X%02X%02X%02X","ESP",
        eth_mac[0],eth_mac[1],eth_mac[2],eth_mac[3],eth_mac[4],eth_mac[5]);
    }
    return aliot_mqtt_devicename;
}

/**
 * 获取客户端id
 * @param 无
 * @return 客户端id
 */
char* aliot_get_clientid(void)
{
    return aliot_get_devicename();
}

/**
 * 获取设备秘钥（此秘钥第一次上电需通过动态注册获取）
 * @param 无
 * @return 设备秘钥
 */
char* aliot_get_devicesecret(void)
{
    static char is_first_load = 0;
    if(!is_first_load)
    {
        size_t required_size = 0;
        
        nvs_handle_t handle;
        ESP_ERROR_CHECK(nvs_open(ALIOT_NVS_NAMESPACE, NVS_READWRITE,&handle));
        nvs_get_str(handle, ALIOT_DEVICESECRET_ITEM, NULL, &required_size);
        if(required_size  > 2)
        {
            nvs_get_str(handle, ALIOT_DEVICESECRET_ITEM, s_device_secret, &required_size);
            ESP_LOGI(TAG,"get secret,len:%d,value:%s",required_size,s_device_secret);
        }
        /*
        if(required_size)
        {
            char *tmp = (char*)malloc(required_size);
            nvs_get_str(handle, ALIOT_DEVICESECRET_ITEM, tmp, &required_size);

            printf("get tmp:");
            for(int i = 0;i < required_size;i++)
            {
                printf("%c",tmp[i]);
            }
            printf("\n");
        }
        */
        is_first_load = 1;
        nvs_close(handle);
    }
    return s_device_secret;
}

/**
 * 设置设备秘钥
 * @param secret 秘钥
 * @return 无
 */
void aliot_set_devicesecret(char *secret)
{
    nvs_handle dev_nvs;
    ESP_ERROR_CHECK(nvs_open(ALIOT_NVS_NAMESPACE, NVS_READWRITE, &dev_nvs));
    esp_err_t err = nvs_set_str(dev_nvs, ALIOT_DEVICESECRET_ITEM, secret);
    if (err != ESP_OK) {
        return ;
    }
    ESP_LOGI(TAG,"write secret:%s",secret);
    snprintf(s_device_secret,sizeof(s_device_secret),"%s",secret);
    nvs_commit(dev_nvs);
    nvs_close(dev_nvs);
}

/**
 * 计算hmd5
 * @param key 秘钥
 * @param content 内容
 * @param output 输出md5值
 * @return 无
 */
void calc_hmd5(char* key,char *content,char *output)
{
    mbedtls_md_context_t md5_ctx;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);

    mbedtls_md_init(&md5_ctx);
    mbedtls_md_setup(&md5_ctx, md_info, 1);
    mbedtls_md_hmac_starts(&md5_ctx,key,strlen(key));
    mbedtls_md_hmac_update(&md5_ctx,content,strlen(content));
    mbedtls_md_hmac_finish(&md5_ctx,output);
    mbedtls_md_free(&md5_ctx);
}

/**
 * hex转str
 * @param input 输入
 * @param input_len 输入长度
 * @param output 输出
 * @param lowercase 0:大小，1:小写
 * @return 无
 */
void core_hex2str(uint8_t *input, uint32_t input_len, char *output, uint8_t lowercase)
{
    char *upper = "0123456789ABCDEF";
    char *lower = "0123456789abcdef";
    char *encode = upper;
    int i = 0, j = 0;

    if (lowercase) {
        encode = lower;
    }

    for (i = 0; i < input_len; i++) {
        output[j++] = encode[(input[i] >> 4) & 0xf];
        output[j++] = encode[(input[i]) & 0xf];
    }
    output[j] = 0;
}

//根证书
const char* g_aliot_ca = "-----BEGIN CERTIFICATE-----\n"
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG"
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv"
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw"
"MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i"
"YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT"
"aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ"
"jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp"
"xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp"
"1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG"
"snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ"
"U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8"
"9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E"
"BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B"
"AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz"
"yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE"
"38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP"
"AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad"
"DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME"
"HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n"
"-----END CERTIFICATE-----";

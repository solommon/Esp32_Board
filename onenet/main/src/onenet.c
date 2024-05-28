#include "onenet.h"
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
#include "base64.h"

#define TAG     "onenet"

#define ONENET_NVS_NAMESPACE     "onenet"

#define ONENET_DEVICESECRET_ITEM     "devSecret"

//设备秘钥
static char s_device_secret[128] = { 0 };

/**
 * 获取设备名称
 * @param 无
 * @return 设备名称
 */
char* onenet_get_devicename(void)
{
    static char onenet_mqtt_devicename[64] = {0};
    if(onenet_mqtt_devicename[0] == 0)
    {
        esp_err_t esp_code = ESP_OK;
        uint8_t eth_mac[6];
        esp_code = esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
        if(esp_code != ESP_OK)
            memset(eth_mac,0,sizeof(eth_mac));
        snprintf(onenet_mqtt_devicename,64,"%s%02X%02X%02X%02X%02X%02X","ESP",
        eth_mac[0],eth_mac[1],eth_mac[2],eth_mac[3],eth_mac[4],eth_mac[5]);
    }
    return onenet_mqtt_devicename;
}

/**
 * 获取客户端id
 * @param 无
 * @return 客户端id
 */
char* onenet_get_clientid(void)
{
    return onenet_get_devicename();
}

/**
 * 获取设备秘钥（此秘钥第一次上电需通过动态注册获取）
 * @param 无
 * @return 设备秘钥
 */
char* onenet_get_devicesecret(void)
{
    static char is_first_load = 0;
    if(!is_first_load)
    {
        size_t required_size = 0;
        
        nvs_handle_t handle;
        ESP_ERROR_CHECK(nvs_open(ONENET_NVS_NAMESPACE, NVS_READWRITE,&handle));
        nvs_get_str(handle, ONENET_DEVICESECRET_ITEM, NULL, &required_size);
        if(required_size  > 2)
        {
            nvs_get_str(handle, ONENET_DEVICESECRET_ITEM, s_device_secret, &required_size);
            ESP_LOGI(TAG,"get secret,len:%d,value:%s",required_size,s_device_secret);
        }
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
void onenet_set_devicesecret(char *secret)
{
    nvs_handle dev_nvs;
    ESP_ERROR_CHECK(nvs_open(ONENET_NVS_NAMESPACE, NVS_READWRITE, &dev_nvs));
    esp_err_t err = nvs_set_str(dev_nvs, ONENET_DEVICESECRET_ITEM, secret);
    if (err != ESP_OK) {
        return ;
    }
    ESP_LOGI(TAG,"write secret:%s",secret);
    snprintf(s_device_secret,sizeof(s_device_secret),"%s",secret);
    nvs_commit(dev_nvs);
    nvs_close(dev_nvs);
}

/**
 * 计算hmd
 * @param key 秘钥
 * @param content 内容
 * @param output 输出md5值
 * @return 无
 */
void calc_hmd(enum sig_method_e method,unsigned char* key,size_t key_len,unsigned char *content,size_t content_len,unsigned char *output)
{
    mbedtls_md_context_t md_ctx;
    const mbedtls_md_info_t *md_info = NULL;
    if (SIG_METHOD_MD5 == method) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    } else if (SIG_METHOD_SHA1 == method) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    } else if (SIG_METHOD_SHA256 == method) {
        md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    }

    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, md_info, 1);
    mbedtls_md_hmac_starts(&md_ctx,key,strlen(key));
    mbedtls_md_hmac_update(&md_ctx,content,strlen(content));
    mbedtls_md_hmac_finish(&md_ctx,output);
    mbedtls_md_free(&md_ctx);
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

#define DEV_TOKEN_LEN 256
#define DEV_TOKEN_VERISON_STR "2018-10-31"

#define DEV_TOKEN_SIG_METHOD_MD5 "md5"
#define DEV_TOKEN_SIG_METHOD_SHA1 "sha1"
#define DEV_TOKEN_SIG_METHOD_SHA256 "sha256"

//计算token
int32_t
dev_token_generate(uint8_t* token, enum sig_method_e method, uint32_t exp_time, const uint8_t* product_id, const uint8_t* dev_name, const uint8_t* access_key)
{

    uint8_t  base64_data[64] = { 0 };
    uint8_t  str_for_sig[64] = { 0 };
    uint8_t  sign_buf[128]   = { 0 };
    unsigned int base64_data_len = sizeof(base64_data);
    uint8_t* sig_method_str  = NULL;
    unsigned int sign_len        = 0;
    uint32_t i               = 0;
    uint8_t* tmp             = NULL;

    sprintf(token, (const uint8_t*)"version=%s", DEV_TOKEN_VERISON_STR);

    if (dev_name) {
        sprintf(token + strlen(token), (const uint8_t*)"&res=products%%2F%s%%2Fdevices%%2F%s", product_id, dev_name);
    } else {
        sprintf(token + strlen(token), (const uint8_t*)"&res=products%%2F%s", product_id);
    }

    sprintf(token + strlen(token), (const uint8_t*)"&et=%lu", exp_time);

    Base64_Decode(access_key, strlen(access_key), base64_data, &base64_data_len);

    if (SIG_METHOD_MD5 == method) {
        sig_method_str = (uint8_t*)DEV_TOKEN_SIG_METHOD_MD5;
        sign_len       = 16;
    } else if (SIG_METHOD_SHA1 == method) {
        sig_method_str = (uint8_t*)DEV_TOKEN_SIG_METHOD_SHA1;
        sign_len       = 20;
    } else if (SIG_METHOD_SHA256 == method) {
        sig_method_str = (uint8_t*)DEV_TOKEN_SIG_METHOD_SHA256;
        sign_len       = 32;
    }

    sprintf(token + strlen(token), (const uint8_t*)"&method=%s", sig_method_str);
    if (dev_name) {
        sprintf(str_for_sig, (const uint8_t*)"%lu\n%s\nproducts/%s/devices/%s\n%s", exp_time, sig_method_str, product_id, dev_name, DEV_TOKEN_VERISON_STR);
    } else {
        sprintf(str_for_sig, (const uint8_t*)"%lu\n%s\nproducts/%s\n%s", exp_time, sig_method_str, product_id, DEV_TOKEN_VERISON_STR);
    }

    calc_hmd(method,base64_data,base64_data_len,str_for_sig,strlen(str_for_sig),sign_buf);

    memset(base64_data, 0, sizeof(base64_data));
    base64_data_len = sizeof(base64_data);
    Base64_Encode_NoNl(sign_buf, sign_len, base64_data, &base64_data_len);

    strcat(token, (const uint8_t*)"&sign=");
    tmp = token + strlen(token);

    for (i = 0; i < base64_data_len; i++) {
        switch (base64_data[i]) {
            case '+':
                strcat(tmp, (const uint8_t*)"%2B");
                tmp += 3;
                break;
            case ' ':
                strcat(tmp, (const uint8_t*)"%20");
                tmp += 3;
                break;
            case '/':
                strcat(tmp, (const uint8_t*)"%2F");
                tmp += 3;
                break;
            case '?':
                strcat(tmp, (const uint8_t*)"%3F");
                tmp += 3;
                break;
            case '%':
                strcat(tmp, (const uint8_t*)"%25");
                tmp += 3;
                break;
            case '#':
                strcat(tmp, (const uint8_t*)"%23");
                tmp += 3;
                break;
            case '&':
                strcat(tmp, (const uint8_t*)"%26");
                tmp += 3;
                break;
            case '=':
                strcat(tmp, (const uint8_t*)"%3D");
                tmp += 3;
                break;
            default:
                *tmp = base64_data[i];
                tmp += 1;
                break;
        }
    }

    return 0;
}


//根证书
const char* g_onenet_ca = "-----BEGIN CERTIFICATE-----\n"
"MIIDOzCCAiOgAwIBAgIJAPCCNfxANtVEMA0GCSqGSIb3DQEBCwUAMDQxCzAJBgNV"
"BAYTAkNOMQ4wDAYDVQQKDAVDTUlPVDEVMBMGA1UEAwwMT25lTkVUIE1RVFRTMB4X"
"DTE5MDUyOTAxMDkyOFoXDTQ5MDUyMTAxMDkyOFowNDELMAkGA1UEBhMCQ04xDjAM"
"BgNVBAoMBUNNSU9UMRUwEwYDVQQDDAxPbmVORVQgTVFUVFMwggEiMA0GCSqGSIb3"
"DQEBAQUAA4IBDwAwggEKAoIBAQC/VvJ6lGWfy9PKdXKBdzY83OERB35AJhu+9jkx"
"5d4SOtZScTe93Xw9TSVRKrFwu5muGgPusyAlbQnFlZoTJBZY/745MG6aeli6plpR"
"r93G6qVN5VLoXAkvqKslLZlj6wXy70/e0GC0oMFzqSP0AY74icANk8dUFB2Q8usS"
"UseRafNBcYfqACzF/Wa+Fu/upBGwtl7wDLYZdCm3KNjZZZstvVB5DWGnqNX9HkTl"
"U9NBMS/7yph3XYU3mJqUZxryb8pHLVHazarNRppx1aoNroi+5/t3Fx/gEa6a5PoP"
"ouH35DbykmzvVE67GUGpAfZZtEFE1e0E/6IB84PE00llvy3pAgMBAAGjUDBOMB0G"
"A1UdDgQWBBTTi/q1F2iabqlS7yEoX1rbOsz5GDAfBgNVHSMEGDAWgBTTi/q1F2ia"
"bqlS7yEoX1rbOsz5GDAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAL"
"aqJ2FgcKLBBHJ8VeNSuGV2cxVYH1JIaHnzL6SlE5q7MYVg+Ofbs2PRlTiWGMazC7"
"q5RKVj9zj0z/8i3ScWrWXFmyp85ZHfuo/DeK6HcbEXJEOfPDvyMPuhVBTzuBIRJb"
"41M27NdIVCdxP6562n6Vp0gbE8kN10q+ksw8YBoLFP0D1da7D5WnSV+nwEIP+F4a"
"3ZX80bNt6tRj9XY0gM68mI60WXrF/qYL+NUz+D3Lw9bgDSXxpSN8JGYBR85BxBvR"
"NNAhsJJ3yoAvbPUQ4m8J/CoVKKgcWymS1pvEHmF47pgzbbjm5bdthlIx+swdiGFa"
"WzdhzTYwVkxBaU+xf/2w\n"
"-----END CERTIFICATE-----";

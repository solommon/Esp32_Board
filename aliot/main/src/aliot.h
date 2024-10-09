#ifndef _ALIOT_H_
#define _ALIOT_H_
#include <stdint.h>

//产品KEY
#define  ALIOT_PRODUCT_KEY  "a1aZYOu6wEW"

//产品SECRET
#define  ALIOT_PRODUCT_SECRET "XBy6j3XshVnffaTO"

//设备名称
#define  ALIOT_DEVICE_NAME  "ESP48E729B67324"

//YourRegionId 北京:cn-beijing;上海:cn-shanghai;深圳:cn-shenzhen
#define  ALIOT_REGION_ID  "cn-shanghai"

//是否使用动态注册的方式
//#define ALIOT_USE_REGISTER

//设备秘钥
#ifndef ALIOT_USE_REGISTER
#define ALIOT_DEVICE_SECRET     "f307c7fffa4ec41a7c17e9da699ce351"
#endif

/**
 * 获取设备名称
 * @param 无
 * @return 设备名称
 */
char* aliot_get_devicename(void);

/**
 * 获取客户端id
 * @param 无
 * @return 客户端id
 */
char* aliot_get_clientid(void);

/**
 * 获取设备秘钥（此秘钥第一次上电需通过动态注册获取）
 * @param 无
 * @return 设备秘钥
 */
char* aliot_get_devicesecret(void);

/**
 * 设置设备秘钥
 * @param secret 秘钥
 * @return 无
 */
void aliot_set_devicesecret(char *secret);

/**
 * 计算hmd5
 * @param key 秘钥
 * @param content 内容
 * @param output 输出md5值
 * @return 无
 */
void calc_hmd5(char* key,char *content,char *output);

/**
 * hex转str
 * @param input 输入
 * @param input_len 输入长度
 * @param output 输出
 * @param lowercase 0:大小，1:小写
 * @return 无
 */
void core_hex2str(uint8_t *input, uint32_t input_len, char *output, uint8_t lowercase);


//根证书
extern const char* g_aliot_ca;

#endif

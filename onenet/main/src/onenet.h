#ifndef _ONENET_H_
#define _ONENET_H_
#include <stdint.h>
#include <stddef.h>

//产品ID
#define  ONENET_PRODUCT_KEY  "4jN51WLYWL"

//产品秘钥
#define  ONENET_ACCESS_KEY  "xc+PS3/bNRHLuXWlg39Lv71FKcVlr2Nnyc2R6UNdI+0="

//token有效时间（2030年1月1日）
#define TM_EXPIRE_TIME 1924833600

enum sig_method_e
{
    SIG_METHOD_MD5,
    SIG_METHOD_SHA1,
    SIG_METHOD_SHA256
};

/**
 * 获取设备名称
 * @param 无
 * @return 设备名称
 */
char* onenet_get_devicename(void);

/**
 * 获取客户端id
 * @param 无
 * @return 客户端id
 */
char* onenet_get_clientid(void);

/**
 * 获取设备秘钥（此秘钥第一次上电需通过动态注册获取）
 * @param 无
 * @return 设备秘钥
 */
char* onenet_get_devicesecret(void);

/**
 * 设置设备秘钥
 * @param secret 秘钥
 * @return 无
 */
void onenet_set_devicesecret(char *secret);

/**
 * 计算hmd
 * @param key 秘钥
 * @param content 内容
 * @param output 输出md5值
 * @return 无
 */
void calc_hmd(enum sig_method_e type,unsigned char* key,size_t key_len,unsigned char *content,size_t content_len,unsigned char *output);

/**
 * hex转str
 * @param input 输入
 * @param input_len 输入长度
 * @param output 输出
 * @param lowercase 0:大小，1:小写
 * @return 无
 */
void core_hex2str(uint8_t *input, uint32_t input_len, char *output, uint8_t lowercase);

//计算token
int32_t
dev_token_generate(uint8_t* token, enum sig_method_e method, uint32_t exp_time, const uint8_t* product_id, const uint8_t* dev_name, const uint8_t* access_key);

//根证书
extern const char* g_onenet_ca;

#endif

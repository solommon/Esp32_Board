#ifndef _ONENET_DM_
#define _ONENET_DM_
#include "cJSON.h"

//处理物模型相关函数

typedef enum
{
    ONENET_DM_POST,      //常规属性上报
    ONENET_DM_SET_ACK,   //属性设置回复
    ONENET_DM_GET_ACK,   //属性获取回复
    ONENET_DM_EVENT,     //事件上报
}ONENET_DM_TYPE;

//物模型
typedef struct
{
    cJSON* dm_js;
    char*  dm_js_str;
    int    data_len;
}ONENET_DM_DES;

/**
 * 生成一dm
 * @param 无
 * @return dm 
 */
ONENET_DM_DES* onenet_malloc_dm(ONENET_DM_TYPE dm_type);


/**
 * 往物模型(dm)里面添加一浮点值
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_double(ONENET_DM_DES* dm,const char* name,double value);

/**
 * 往物模型(dm)里面添加一整形
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_int(ONENET_DM_DES* dm,const char* name,int value);

/**
 * 往物模型(dm)里面添加一字符串
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_str(ONENET_DM_DES* dm,const char* name,const char* value);

/**
 * 往物模型(dm)里面添加一布尔
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_bool(ONENET_DM_DES* dm,const char* name,char value);

/**
 * 往物模型(dm)里面添加一json
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param json 格式的字符串
 * @return 无
 */
void onenet_set_dm_json(ONENET_DM_DES* dm,const char* name,const char* json);

/**
 * 添加设置属性回复
 * @param code 错误码
 * @param message 信息
 * @return 无
 */
void onenet_set_property_ack(ONENET_DM_DES* dm,int code,const char* message);

/**
 * 添加属性设置请求回复(信息)
 * @param code 错误码
 * @param message 信息
 * @return 无
 */
void onenet_get_property_ack_code(ONENET_DM_DES* dm,int code,const char* message);

/**
 * 添加属性设置请求回复(整形)
 * @param name 数据名称
 * @param value 值
 * @return 无
 */
void onenet_get_property_ack_int(ONENET_DM_DES* dm,const char* name,int value);

/**
 * 添加属性设置请求回复（浮点）
 * @param name 数据名称
 * @param value 值
 * @return 无
 */
void onenet_get_property_ack_double(ONENET_DM_DES* dm,const char* name,double value);

/**
 * 添加属性设置请求回复（字符串）
 * @param name 数据名称
 * @param value 值
 * @return 无
 */
void onenet_get_property_ack_str(ONENET_DM_DES* dm,const char* name,const char* value);

/**
 * 设置事件上报todo
 * @param event 事件
 * @param errorcode 事件错误码
 * @return 无
 */
void onenet_set_property_str(ONENET_DM_DES* dm,const char* event,const char* errorcode);

/**
 * 生成字符串保存在dm->dm_js_str
 * @param dm 无
 * @return 无
 */
void onenet_dm_serialize(ONENET_DM_DES* dm);


/**
 * 释放一dm
 * @param 无
 * @return dm 
 */
void onenet_free_dm(ONENET_DM_DES* dm);

#endif

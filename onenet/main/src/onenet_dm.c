#include "onenet_dm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// 记录每个消息的ID
static uint32_t s_onenet_id = 0;

/**
 * 生成一dm
 * @param 无
 * @return dm
 */
ONENET_DM_DES *onenet_malloc_dm(ONENET_DM_TYPE dm_type)
{
    ONENET_DM_DES *dm = (ONENET_DM_DES *)malloc(sizeof(ONENET_DM_DES));
    if (dm)
    {
        memset(dm, 0, sizeof(ONENET_DM_DES));
        dm->dm_js = cJSON_CreateObject();
        char id[10];
        snprintf(id, 10, "%lu", s_onenet_id++);
        cJSON_AddStringToObject(dm->dm_js, "id", id);

        switch (dm_type)
        {
        case ONENET_DM_POST: // 常规属性上报
            cJSON_AddStringToObject(dm->dm_js, "version", "1.0");
            cJSON_AddObjectToObject(dm->dm_js, "params");
            break;
        case ONENET_DM_SET_ACK: // 属性设置回复
            break;
        case ONENET_DM_GET_ACK: // 属性获取回复
            cJSON_AddObjectToObject(dm->dm_js, "data");
            break;
        case ONENET_DM_EVENT: // 事件上报
            cJSON_AddStringToObject(dm->dm_js, "version", "1.0");
            cJSON_AddObjectToObject(dm->dm_js, "params");
            break;
        default:
            break;
        }

        return dm;
    }
    return NULL;
}

/**
 * 往物模型(dm)里面添加一浮点值
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_double(ONENET_DM_DES *dm, const char *name, double value)
{
    if (dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js, "params");
        if (param_js)
        {
            cJSON *name_js = cJSON_AddObjectToObject(param_js, name);
            cJSON_AddNumberToObject(name_js, "value", value);
            cJSON_AddNumberToObject(name_js, "time", time(0) * 1000ull);
        }
    }
}

/**
 * 往物模型(dm)里面添加一整形
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_int(ONENET_DM_DES *dm, const char *name, int value)
{
    if (dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js, "params");
        if (param_js)
        {
            cJSON *name_js = cJSON_AddObjectToObject(param_js, name);
            cJSON_AddNumberToObject(name_js, "value", value);
            cJSON_AddNumberToObject(name_js, "time", time(0) * 1000ull);
        }
    }
}

/**
 * 往物模型(dm)里面添加一字符串
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_str(ONENET_DM_DES *dm, const char *name, const char *value)
{
    if (dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js, "params");
        if (param_js)
        {
            cJSON *name_js = cJSON_AddObjectToObject(param_js, name);
            cJSON_AddStringToObject(name_js, "value", value);
            cJSON_AddNumberToObject(name_js, "time", time(0) * 1000ull);
        }
    }
}

/**
 * 往物模型(dm)里面添加一布尔
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param value 值
 * @return 无
 */
void onenet_set_dm_bool(ONENET_DM_DES* dm,const char* name,char value)
{
    if (dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js, "params");
        if (param_js)
        {
            cJSON *name_js = cJSON_AddObjectToObject(param_js, name);
            cJSON_AddBoolToObject(name_js,"value",value);
            cJSON_AddNumberToObject(name_js, "time", time(0) * 1000ull);
        }
    }
}

/**
 * 往物模型(dm)里面添加一json
 * @param dm 物模型结构体
 * @param name 属性名字
 * @param json 格式的字符串
 * @return 无
 */
void onenet_set_dm_json(ONENET_DM_DES *dm, const char *name, const char *json)
{
    if (dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js, "params");
        if (param_js)
        {
            cJSON *js = cJSON_Parse(json);
            if (js)
            {
                cJSON *name_js = cJSON_AddObjectToObject(param_js, name);
                cJSON_AddItemToObject(name_js, "value", js);
                cJSON_AddNumberToObject(name_js, "time", time(0) * 1000ull);
            }
        }
    }
}

/**
 * 添加设置属性回复
 * @param code 错误码
 * @param message 信息
 * @return 无
 */
void onenet_set_property_ack(ONENET_DM_DES *dm, int code, const char *message)
{
    if (dm)
    {
        cJSON_AddNumberToObject(dm->dm_js, "code", code);
        cJSON_AddStringToObject(dm->dm_js, "msg", message);
    }
}

/**
 * 添加属性设置请求回复(信息)
 * @param code 错误码
 * @param message 信息
 * @return 无
 */
void onenet_get_property_ack_code(ONENET_DM_DES *dm, int code, const char *message)
{
    if (dm)
    {
        cJSON_AddNumberToObject(dm->dm_js, "code", code);
        cJSON_AddStringToObject(dm->dm_js, "msg", message);
    }
}

/**
 * 添加属性设置请求回复(整形)
 * @param name 数据名称
 * @param value 值
 * @return 无
 */
void onenet_get_property_ack_int(ONENET_DM_DES *dm, const char *name, int value)
{
    if (dm)
    {
        cJSON *data_js = cJSON_GetObjectItem(dm->dm_js, "data");
        if (data_js)
        {
            cJSON_AddNumberToObject(data_js, "name", value);
        }
    }
}

/**
 * 添加属性设置请求回复（浮点）
 * @param name 数据名称
 * @param value 值
 * @return 无
 */
void onenet_get_property_ack_double(ONENET_DM_DES *dm, const char *name, double value)
{
    if (dm)
    {
        cJSON *data_js = cJSON_GetObjectItem(dm->dm_js, "data");
        if (data_js)
        {
            cJSON_AddNumberToObject(data_js, "name", value);
        }
    }
}

/**
 * 添加属性设置请求回复（字符串）
 * @param name 数据名称
 * @param value 值
 * @return 无
 */
void onenet_get_property_ack_str(ONENET_DM_DES *dm, const char *name, const char *value)
{
    if (dm)
    {
        cJSON *data_js = cJSON_GetObjectItem(dm->dm_js, "data");
        if (data_js)
        {
            cJSON_AddStringToObject(data_js, "name", value);
        }
    }
}

/**
 * 设置事件上报todo
 * @param event 事件
 * @param errorcode 事件错误码
 * @return 无
 */
void onenet_set_event_report(ONENET_DM_DES *dm, const char *event, const char *errorcode)
{
    if (dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js, "params");
        if (param_js)
        {
            cJSON *value_js = cJSON_AddObjectToObject(param_js, "identifier");
            cJSON_AddNumberToObject(param_js, "time", time(0) * 1000ull);
        }
    }
}

/**
 * 生成字符串保存在dm->dm_js_str
 * @param dm 无
 * @return 无
 */
void onenet_dm_serialize(ONENET_DM_DES *dm)
{
    if (dm->dm_js_str)
    {
        cJSON_free(dm->dm_js_str);
        dm->dm_js_str = NULL;
    }
    dm->dm_js_str = cJSON_PrintUnformatted(dm->dm_js);
    dm->data_len = strlen(dm->dm_js_str);
}

/**
 * 释放一dm
 * @param 无
 * @return dm
 */
void onenet_free_dm(ONENET_DM_DES *dm)
{
    if (dm)
    {
        if (dm->dm_js_str)
        {
            cJSON_free(dm->dm_js_str);
        }
        if (dm->dm_js)
        {
            cJSON_Delete(dm->dm_js);
        }
        free(dm);
    }
}

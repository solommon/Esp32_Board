#include "aliot_dm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//记录每个消息的ID
static uint32_t s_aliot_id = 0;

//是否带时间戳
#define DM_INCLUDE_TIMESTAMP

/**
 * 生成一dm
 * @param 无
 * @return dm 
 */
ALIOT_DM_DES* aliot_malloc_dm(ALIOT_DM_TYPE dm_type)
{
    ALIOT_DM_DES* dm = (ALIOT_DM_DES*)malloc(sizeof(ALIOT_DM_DES));
    if(dm)
    {
        memset(dm,0,sizeof(ALIOT_DM_DES));
        dm->dm_js = cJSON_CreateObject();
        char id[10];
        snprintf(id,10,"%lu",s_aliot_id++);
        cJSON_AddStringToObject(dm->dm_js,"id",id);
        cJSON_AddStringToObject(dm->dm_js,"version","1.0");
        switch(dm_type)
        {
            case ALIOT_DM_POST:      //常规属性上报
                cJSON_AddObjectToObject(dm->dm_js,"params");
                cJSON_AddStringToObject(dm->dm_js,"method","hing.event.property.post");
                break;
            case ALIOT_DM_SET_ACK:   //属性设置回复
                cJSON_AddObjectToObject(dm->dm_js,"data");
                break;
            case ALIOT_DM_EVENT:     //事件上报
                cJSON_AddObjectToObject(dm->dm_js,"params");
                break;
            default:break;
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
void aliot_set_dm_double(ALIOT_DM_DES* dm,const char* name,double value)
{
    if(dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js,"params");
        if(param_js)
        {
            #ifdef DM_INCLUDE_TIMESTAMP
            cJSON *name_js = cJSON_AddObjectToObject(param_js,name);
            cJSON_AddNumberToObject(name_js,"value",value);
            cJSON_AddNumberToObject(name_js,"time",time(0)*1000ull);
            #else
            cJSON_AddNumberToObject(param_js,name,value);
            #endif
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
void aliot_set_dm_int(ALIOT_DM_DES* dm,const char* name,int value)
{
    if(dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js,"params");
        if(param_js)
        {
            #ifdef DM_INCLUDE_TIMESTAMP
            cJSON *name_js = cJSON_AddObjectToObject(param_js,name);
            cJSON_AddNumberToObject(name_js,"value",value);
            cJSON_AddNumberToObject(name_js,"time",time(0)*1000ull);
            #else
            cJSON_AddNumberToObject(param_js,name,value);
            #endif
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
void aliot_set_dm_str(ALIOT_DM_DES* dm,const char* name,const char* value)
{
    if(dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js,"params");
        if(param_js)
        {
            #ifdef DM_INCLUDE_TIMESTAMP
            cJSON *name_js = cJSON_AddObjectToObject(param_js,name);
            cJSON_AddStringToObject(name_js,"value",value);
            cJSON_AddNumberToObject(name_js,"time",time(0)*1000ull);
            #else
            cJSON_AddStringToObject(param_js,name,value);
            #endif
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
void aliot_set_dm_json(ALIOT_DM_DES* dm,const char* name,const char* json)
{
    if(dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js,"params");
        if(param_js)
        {
            cJSON *js = cJSON_Parse(json);
            if(js)
            {
                #ifdef DM_INCLUDE_TIMESTAMP
                cJSON *name_js = cJSON_AddObjectToObject(param_js,name);
                cJSON_AddItemToObject(name_js,"value",js);
                cJSON_AddNumberToObject(name_js,"time",time(0)*1000ull);
                #else
                cJSON_AddItemToObject(param_js,"value",js);
                #endif
            }
        }
    }
}

/**
 * 添加属性设置回复
 * @param code 错误码
 * @param message 信息
 * @return 无
 */
void aliot_set_property_ack(ALIOT_DM_DES* dm,int code,const char* message)
{
    if(dm)
    {
        cJSON_AddNumberToObject(dm->dm_js,"code",code);
        cJSON_AddStringToObject(dm->dm_js,"message",message);
    }
}

/**
 * 设置事件上报
 * @param event 事件
 * @param errorcode 事件错误码
 * @return 无
 */
void aliot_set_event_report(ALIOT_DM_DES* dm,const char* event,const char* errorcode)
{
    if(dm)
    {
        cJSON *param_js = cJSON_GetObjectItem(dm->dm_js,"params");
        if(param_js)
        {
            cJSON *value_js = cJSON_AddObjectToObject(param_js,"value");
            cJSON_AddObjectToObject(value_js,"errorcode");
            cJSON_AddStringToObject(value_js,"errorcode",errorcode);
            cJSON_AddNumberToObject(param_js,"time",time(0)*1000ull);

            char method[128];
            snprintf(method,sizeof(method),"thing.event.%s.post",event);
            cJSON_AddStringToObject(dm->dm_js,"method",method);
        }
    }
}

/**
 * 生成字符串保存在dm->dm_js_str
 * @param dm 无
 * @return 无
 */
void aliot_dm_serialize(ALIOT_DM_DES* dm)
{
    if(dm->dm_js_str)
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
void aliot_free_dm(ALIOT_DM_DES* dm)
{
    if(dm)
    {
        if(dm->dm_js_str)
        {
            cJSON_free(dm->dm_js_str);
        }
        if(dm->dm_js)
        {
            cJSON_Delete(dm->dm_js);
        }
        free(dm);
    }
}

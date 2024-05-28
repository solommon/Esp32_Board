#ifndef _ONENET_MQTT_H_
#define _ONENET_MQTT_H_

//用于后台下发的值
typedef union
{
    int i_value;
    double f_value;
    char* s_value;
}onenet_value;

//属性设置回调函数
typedef void(*fp_property_cb)(const char* name,onenet_value value);

/**
 * 启动onenet连接（启动前请确保wifi连接正常，产品id和产品秘钥已正确修改）
 * @param 无
 * @return 无
 */
void onenet_start(void);

/**
 * 设置属性下发控制回调函数
 * @param f 回调函数 
 * @return 无
 */
void onenet_set_property_cb(fp_property_cb f);

/**
 * 上报单个属性值（浮点）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_double(const char* name,double value);

/**
 * 上报单个属性值（整形）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_int(const char* name,int value);

/**
 * 上报单个属性值（字符串）
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_str(const char* name,const char* value);

/**
 * 上报整个json
 * @param name 属性值名字
 * @param value 值
 * @return 无
 */
void onenet_post_property_data(const char* js);



#endif

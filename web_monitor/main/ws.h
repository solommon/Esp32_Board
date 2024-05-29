#ifndef _WS_H_
#define _WS_H_
#include "esp_err.h"

//ws接收到的处理回调函数
typedef void(*ws_receive_cb)(uint8_t* payload,int len);

//ws周期需要发送的函数数据
typedef void(*ws_send_cb)(char* send_buf,int *len);

typedef struct
{
    const char* html_code;              //当执行http访问时返回的html页面
    ws_receive_cb   receive_fn;         //当ws接收到数据时，调用此函数
    ws_send_cb      send_fn;            //周期发送数据
    int         intervel_ms;            //周期发送数据周期(需不小于1000ms)
}ws_cfg_t;

/** 初始化ws
 * @param cfg ws一些配置,请看ws_cfg_t定义
 * @return  ESP_OK or ESP_FAIL
*/
esp_err_t   web_monitor_init(ws_cfg_t *cfg);

#endif

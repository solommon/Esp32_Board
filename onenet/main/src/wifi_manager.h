#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#include <stdbool.h>


//WIFI MANAGER相关事件
typedef enum {
    WIFI_EV_READY,              //WIFI MANAGER初始化完成
    WIFI_EV_CONNECT_SUCCESS,    //获取IP成功
    WIFI_EV_SNTP,               //EVSNTP联网成功
    WIFI_EV_CONNECT_TIMEOUT,    //连接超时
    WIFI_EV_DISCONNECTED,       //断开连接
    WIFI_EV_MAX,
} wifi_ev_t;

//WIFI相关事件回调函数
typedef void (*wifi_manager_callback)(wifi_ev_t ev, void *data);

//初始化
int wifi_manager_init(wifi_manager_callback f);

//启动WIFI连接
int wifi_manager_start(void);

//设置ssid和密码
void wifi_manager_set_ssid(char* ssid,char *password);

#endif

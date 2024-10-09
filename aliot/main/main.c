#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "aliot_mqtt.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "esp_system.h"
#include "cJSON.h"
#include "aliot.h"
#include "aliot_dm.h"
#include "button.h"

#define TAG     "main"

#define EV_WIFI_CONNECTED_BIT           (1<<0)
#define EV_WIFI_DISCONNECTED_BIT        (1<<1)
static EventGroupHandle_t s_wifi_ev;

/**
 * wifi事件回调函数
 * @param ev wifi事件
 * @param data 参数，暂无用处
 * @return 无
 */
void wifi_callback(wifi_ev_t ev, void *data)
{
    if(ev == WIFI_EV_SNTP)
    {
        xEventGroupSetBits(s_wifi_ev,EV_WIFI_CONNECTED_BIT);
    }
    if(ev == WIFI_EV_DISCONNECTED)
    {
        xEventGroupSetBits(s_wifi_ev,EV_WIFI_DISCONNECTED_BIT);
    }
    if(ev == WIFI_EV_READY)
    {
        ESP_LOGI(TAG,"deviceName:%s",aliot_get_devicename());
    }
}

/**
 * 板级初始化，自己在这个函数加初始化代码
 * @param 无
 * @return 无
 */
void board_init(void)
{
    esp_err_t ret_val = ESP_OK;
    ret_val = nvs_flash_init();
    if (ret_val == ESP_ERR_NVS_NO_FREE_PAGES || ret_val == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_val = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret_val );
}

/**
 * 调用此函数会将属性值上报
 * @param 无
 * @return 无
 */
void  aliot_property_report(void)
{
    ALIOT_DM_DES* dm_des = aliot_malloc_dm(ALIOT_DM_POST);

    //在这里添加你需要上报的值，以下是演示值
    aliot_set_dm_int(dm_des,"LightMode",1);     //灯模式
    aliot_set_dm_int(dm_des,"ColorTemperature",5000);   //冷暖色温
    aliot_set_dm_int(dm_des,"LightSwitch",1);     //开关
    aliot_set_dm_json(dm_des,"RGBColor","{\"Red\":120,\"Green\":130,\"Blue\":150}");

    aliot_dm_serialize(dm_des);
    aliot_post_property_data(dm_des->dm_js_str);

    ESP_LOGI(TAG,"dm_des->%s",dm_des->dm_js_str);

    aliot_free_dm(dm_des);
}

//设置WIFI 命令
static int set_wifi_pwd(int argc, char **argv)
{
    if(argc != 3)
    {
        ESP_LOGE(TAG, "Invaild param");
        return -1;
    }
    wifi_manager_set_ssid(argv[1],argv[2]);
    
    return 0;
}

//上报一次值
static int report_property(int argc, char **argv)
{
    aliot_property_report();
    return 0;
}

//注册命令
void set_cmd_init(void)
{
    const esp_console_cmd_t cmd1 = {
        .command = "netcfg",
        .help = "netcfg",
        .hint = NULL,
        .func = set_wifi_pwd,
    };
    esp_console_cmd_register(&cmd1);

    const esp_console_cmd_t cmd2 = {
        .command = "report",
        .help = "report aliot property",
        .hint = NULL,
        .func = report_property,
    };
    esp_console_cmd_register(&cmd2);
}

/**
 * 初始化调试命令行
 * @param 无
 * @return 无
 */
static void initialize_console(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);

    const uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_REF_TICK,
    };
    /* Install UART driver for interrupt-driven reads and writes */
    
    ESP_ERROR_CHECK( uart_param_config(0, &uart_config) );
    ESP_ERROR_CHECK( uart_driver_install(0,256, 0, 0, NULL, 0) );
    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(0);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    //多行显示
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    //历史记录条数
    linenoiseHistorySetMaxLen(100);

    //单条命令长度
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);

    /* Don't return empty lines */
    //linenoiseAllowEmpty(false);

}

/**
 * 调试命令行的任务
 * @param 无
 * @return 无
 */
void cmd_task(void* param)
{
    linenoiseSetDumbMode(1);
    while(true) {
        char* line = linenoise("aliot>");   //命令前缀提示
        if (line == NULL) {
            break;
        }
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);

        }
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}

/**
 * 当后台下发了属性值时，会调用此回调函数
 * @param name 属性值的名字
 * @param value 属性值
 * @return 无
 */
void  aliot_property_cb(const char* name,aliot_value value)
{
    ESP_LOGI(TAG,"set property,property name:%s,value:%.2lf",name,value.f_value);
}


void app_main(void) 
{
    
    //板级初始化
    board_init();
    //命令行初始化
    initialize_console();
    set_cmd_init();
    //wifi初始化
    s_wifi_ev = xEventGroupCreate();
    wifi_manager_init(wifi_callback);
    wifi_manager_start();
    
    aliot_set_property_cb(aliot_property_cb);

    //启动调试命令行任务
    xTaskCreatePinnedToCore(cmd_task, "cmd_task", 4096, NULL,2, NULL, tskNO_AFFINITY);

    while(1)
    {
        //这里等待一个WIFI连接成功的事件，然后再启动aliot连接
        EventBits_t bits = xEventGroupWaitBits(s_wifi_ev,EV_WIFI_CONNECTED_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(5000));
        if(bits&EV_WIFI_CONNECTED_BIT)
        {
            aliot_start();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1*1000));
    }

    while(1)
    {
        //do yourself thing
        vTaskDelay(pdMS_TO_TICKS(1*1000));
    }
}

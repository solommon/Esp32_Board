#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_partition.h"
#include <string.h>

static const char* TAG = "main";

#define USER_PARTITION_TYPE     0x40        //自定义的分区类型
#define USER_PARTITION_SUBTYPE  0x01        //自定义的分区子类型

//分区指针
static const esp_partition_t* partition_res=NULL;

//读取缓存
static char g_esp_buf[1024];

void app_main(void)
{
    //找到自定义分区，返回分区指针，后续用到这个指针进行各种操作
    partition_res=esp_partition_find_first(USER_PARTITION_TYPE,USER_PARTITION_SUBTYPE,NULL);
    if(partition_res != NULL)
    {
        ESP_LOGI(TAG,"Can't find partition,return");
        return;
    }
    //擦除
    ESP_ERROR_CHECK(esp_partition_erase_range(partition_res,0x0,0x1000));
    //测试字符串
    const char* test_str = "this is for test string";
    //从分区偏移位置0x0写入字符串
    ESP_ERROR_CHECK(esp_partition_write(partition_res,0x00, test_str, strlen(test_str)));
    //从分区偏移位置0x0读取字符串
    ESP_ERROR_CHECK(esp_partition_read(partition_res,0x00, g_esp_buf, strlen(test_str)));
    ESP_LOGI(TAG,"Read partition str:%s",g_esp_buf);
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

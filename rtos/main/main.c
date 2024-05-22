#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "main";

/** 任务A
 * @param 无
 * @return 无
*/
void taskA(void *param)
{
    while(1)
    {
        //每隔500ms打印
        ESP_LOGI(TAG,"this is taskA");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
/** 任务B
 * @param 无
 * @return 无
*/
void taskB(void* param)
{
    while(1)
    {
        //每隔700ms打印
        ESP_LOGI(TAG,"this is taskB");
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

/** 任务例程初始化
 * @param 无
 * @return 无
*/
void rtos_task_sample(void)
{
    //以下创建两个任务，任务栈大小为2048字节，优先级为3，并设定运行在CORE1上
    xTaskCreatePinnedToCore(taskA,"taskA",2048,NULL,3,NULL,1);
    xTaskCreatePinnedToCore(taskB,"taskB",2048,NULL,3,NULL,1);
}

//队列句柄
static QueueHandle_t s_testQueue;
//定义一个队列数据内容结构体
typedef struct
{
    int num;    //里面只有一个num成员，用来记录一下数据
}queue_packet;

/** 队列任务A，用于定时向队列发送queue_packet数据
 * @param 无
 * @return 无
*/
void queue_taskA(void *param)
{
    int test_cnt = 0;
    while(1)
    {
        queue_packet packet;
        packet.num = test_cnt++;
        //发送queue_packet数据
        xQueueSend(s_testQueue,&packet,pdMS_TO_TICKS(200));
        ESP_LOGI(TAG,"taskA send packet,num:%d",packet.num);
        //延时1000ms
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/** 队列任务B，用于从队列接收数据
 * @param 无
 * @return 无
*/
void queue_taskB(void *param)
{
    while(1)
    {
        queue_packet packet;
        BaseType_t ret = xQueueReceive(s_testQueue,&packet,pdMS_TO_TICKS(200));
        if(ret == pdTRUE)
        {
            //如果收到数据就打印出来
            ESP_LOGI(TAG,"taskB receive packet,num:%d",packet.num);
        }
    }
}

/** 初始化队列例程
 * @param 无
 * @return 无
*/
void rtos_queue_sample(void)
{
    //初始化一个队列，队列单元内容是queue_packet结构体，最大长度是5
    s_testQueue = xQueueCreate(5,sizeof(queue_packet));
    //队列任务A，定时向队列发送数据
    xTaskCreatePinnedToCore(queue_taskA,"queue_taskA",2048,NULL,3,NULL,1);
    //队列任务B，从队列中接收数据
    xTaskCreatePinnedToCore(queue_taskB,"queue_taskB",2048,NULL,3,NULL,1);
}

//二进制信号量
static SemaphoreHandle_t s_testBinSem;
//计数信号量
static SemaphoreHandle_t s_testCountSem;
//互斥信号量
static SemaphoreHandle_t s_testMuxSem;

/** 信号量任务A，定时向三种信号量句柄释放信号
 * @param 无
 * @return 无
*/
void sem_taskA(void* param)
{
    const int count_sem_num = 5;
    while(1)
    {
        //向二值信号量释放信号
        xSemaphoreGive(s_testBinSem);

        //向计数信号量释放5个信号
        for(int i = 0;i < count_sem_num;i++)
        {
            xSemaphoreGive(s_testCountSem);
        }

        //向互斥信号量释放信号
        xSemaphoreGiveRecursive(s_testMuxSem);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void sem_taskB(void* param)
{
    BaseType_t ret = 0;
    while(1)
    {
        //无限等待二进制信号量，直到获取成功才返回
        ret = xSemaphoreTake(s_testBinSem,portMAX_DELAY);
        if(ret == pdTRUE)
            ESP_LOGI(TAG,"take binary semaphore");


        //接收计数信号量，每次接收200ms，直到接收失败才结束循环
        int sem_count = 0;
        do
        {
            ret = xSemaphoreTake(s_testCountSem,pdMS_TO_TICKS(200));
            if(ret==pdTRUE)
            {
                ESP_LOGI(TAG,"take count semaphore,count:%d\r\n",++sem_count);
            }
        }while(ret ==pdTRUE);
        
        //无限等待互斥信号量，直到获取成功才返回，这里用法和二进制信号量极为类似
        ret = xSemaphoreTakeRecursive(s_testMuxSem,portMAX_DELAY);
        if(ret == pdTRUE)
            ESP_LOGI(TAG,"take Mutex semaphore");

    }
}

/** 初始化信号量例程
 * @param 无
 * @return 无
*/
void rtos_sem_sample(void)
{
    s_testBinSem = xSemaphoreCreateBinary();
    s_testCountSem = xSemaphoreCreateCounting(5,0);
    s_testMuxSem = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(sem_taskA,"sem_taskA",2048,NULL,3,NULL,1);
    xTaskCreatePinnedToCore(sem_taskB,"sem_taskB",2048,NULL,3,NULL,1);
}

//事件组句柄
static EventGroupHandle_t s_testEvent;

/** 事件任务A，用于定时标记事件
 * @param 无
 * @return 无
*/
void event_taskA(void* param)
{
    while(1)
    {
        xEventGroupSetBits(s_testEvent,BIT0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        xEventGroupSetBits(s_testEvent,BIT1);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

/** 事件任务B，等待事件组中BIT0和BIT1位
 * @param 无
 * @return 无
*/
void event_taskB(void* param)
{
    EventBits_t ev;
    while(1)
    {
        ev = xEventGroupWaitBits(s_testEvent,BIT0|BIT1,pdTRUE,pdFALSE,portMAX_DELAY);
        if(ev & BIT0)
        {
            ESP_LOGI(TAG,"Event BIT0 set");
        }
        if(ev& BIT1)
        {
            ESP_LOGI(TAG,"Event BIT1 set");
        }
    }
}

/** 事件例程初始化
 * @param 无
 * @return 无
*/
void rtos_event_sample(void)
{
    s_testEvent = xEventGroupCreate();
    xTaskCreatePinnedToCore(event_taskA,"event_taskA",2048,NULL,3,NULL,1);
    xTaskCreatePinnedToCore(event_taskB,"event_taskB",2048,NULL,3,NULL,1);
}

//要使用任务通知，需要记录任务句柄
static TaskHandle_t s_notifyTaskAHandle;
static TaskHandle_t s_notifyTaskBHandle;

/** 任务通知A，用于定时向任务通知B直接传输数据
 * @param 无
 * @return 无
*/
void notify_taskA(void* param)
{
    uint32_t rec_val = 0;
    while(1)
    {
        if (xTaskNotifyWait(0x00, ULONG_MAX, &rec_val, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            ESP_LOGI(TAG,"receive notify value:%lu",rec_val);
        }
    }
}

/** 任务通知B，实时接收任务通知A的数据
 * @param 无
 * @return 无
*/
void notify_taskB(void* param)
{
    int notify_val = 0;
    while(1)
    {
        xTaskNotify(s_notifyTaskAHandle, notify_val, eSetValueWithOverwrite);
        notify_val++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/** 任务通知例程初始化
 * @param 无
 * @return 无
*/
void rtos_notify_sample(void)
{
    xTaskCreatePinnedToCore(notify_taskA,"notify_taskA",2048,NULL,3,&s_notifyTaskAHandle,1);
    xTaskCreatePinnedToCore(notify_taskB,"notify_taskB",2048,NULL,3,&s_notifyTaskBHandle,1);
}

//入口函数
void app_main(void)
{
    /*
    以下是每种freeRTOS特性的测试例程的初始化函数，建议每次只开一个，否则有太多打印影响体验
    */
    //rtos_task_sample();
    //rtos_queue_sample();
    rtos_sem_sample();
    //rtos_event_sample();
    //rtos_notify_sample();
}

#include <stdio.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#define TAG     "uart"

#define     USER_UART_NUM   UART_NUM_2
#define     UART_BUFFER_SIZE        1024

static uint8_t uart_buffer[1024];

static QueueHandle_t uart_queue;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    while (1)
    {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            ESP_LOGI(TAG, "uart[%d] event:", USER_UART_NUM);
            switch (event.type) {
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(USER_UART_NUM, uart_buffer, event.size, portMAX_DELAY);
                ESP_LOGI(TAG, "[DATA EVT]:");
                uart_write_bytes(USER_UART_NUM, uart_buffer, event.size);
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(USER_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(USER_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    uart_config_t uart_cfg={
        .baud_rate=115200,                          //波特率115200
        .data_bits=UART_DATA_8_BITS,                //8位数据位
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,        //无硬件流控制
        .parity=UART_PARITY_DISABLE,                //无校验位
        .stop_bits=UART_STOP_BITS_1                 //1位停止位
    };
    uart_driver_install(USER_UART_NUM,1024,1024,20,&uart_queue,0);     //安装串口驱动
    uart_param_config(USER_UART_NUM,&uart_cfg);
    uart_set_pin(USER_UART_NUM,GPIO_NUM_32,GPIO_NUM_33,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE); //设置引脚,tx为32,rx为33
    
    xTaskCreatePinnedToCore(uart_event_task,"uart",4096,NULL,3,NULL,1);
    #if 0
    while(1)
    {
        int rec = uart_read_bytes(USER_UART_NUM,uart_buffer,sizeof(uart_buffer),pdMS_TO_TICKS(100));
        if(rec)
        {
            uart_write_bytes(USER_UART_NUM,uart_buffer,rec);
        }
    }
    #endif
}   

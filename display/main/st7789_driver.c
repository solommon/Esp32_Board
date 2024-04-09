#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "st7789_driver.h"
#include "lvgl.h"

#define LCD_BL_GPIO     2
#define LCD_MOSI_GPIO   3
#define LCD_CLK_GPIO    4
#define LCD_CS_GPIO     5
#define LCD_DC_GPIO     6
#define LCD_RST_GPIO    7

#define LCD_SPI_FREQUENCE   40*1000*1000

#define LCD_SPI_HOST    SPI2_HOST

#define LCD_H_RES              240
#define LCD_V_RES              320

static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;

typedef struct {
    uint8_t cmd;                //命令
    uint8_t data[16];           //数据
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
    uint8_t delayflg;           //延时标志
    uint16_t delaytime;          //延时时间ms
} lcd_init_cmd_t;

//初始化写入的命令列表
static const lcd_init_cmd_t    s_lcd_cmd_list[] = 
{
    {0x01,{0},0,1,150},
    {0x11,{0},0,1,500},
    {0x3A,{0x55},1,1,10},
    {0x36,{0x00},1,0,0},
    {0x2A,{0,0,LCD_V_RES>>8,LCD_V_RES&0xff},4,0,0},
    {0x2B,{0,0,LCD_H_RES>>8,LCD_H_RES&0xff},4,0,0},
    {0x21,{0},0,1,10},
    {0x13,{0},0,1,10},

    {0x36,{0x70},1,0,0},   //显示方向旋转

    {0x29,{0},0,1,500}
};

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

//st7789外设初始化
void st7789_driver_hw_init(void)
{
    //初始化GPIO(BL)
    gpio_config_t bl_gpio_cfg = 
    {
        .pull_up_en = GPIO_PULLUP_DISABLE,          //禁止上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,      //禁止下拉
        .mode = GPIO_MODE_OUTPUT,                   //输出模式
        .intr_type = GPIO_INTR_DISABLE,             //禁止中断
    };
    gpio_config(&bl_gpio_cfg);
    //初始化SPI
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_CLK_GPIO,
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    //创建基于spi的lcd操作句柄
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_GPIO,
        .cs_gpio_num = LCD_CS_GPIO,
        .pclk_hz = LCD_SPI_FREQUENCE,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        //.on_color_trans_done = example_notify_lvgl_flush_ready,   //刷新完成回调函数
        //.user_ctx = &disp_drv,                                    //回调函数参数
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &lcd_io_handle));

}

//st7789命令初始化
void st7789_driver_cmd_init(void)
{
    int i = 0;
    for(i = 0;i < sizeof(s_lcd_cmd_list)/sizeof(s_lcd_cmd_list[0]);i++)
    {
        esp_lcd_panel_io_tx_param(lcd_io_handle,s_lcd_cmd_list[i].cmd,s_lcd_cmd_list[i].data,s_lcd_cmd_list[i].data);
        if(s_lcd_cmd_list[i].delayflg)
            vTaskDelay(pdMS_TO_TICKS(s_lcd_cmd_list[i].delaytime));
    }
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    int x_start = area->x1;
    int x_end = area->x2;
    int y_start = area->y1;
    int y_end = area->y2;

    // define an area of frame memory where MCU can access
    esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4);
    esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4);
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * 2;
    esp_lcd_panel_io_tx_color(lcd_io_handle, LCD_CMD_RAMWR, color_map, len);

    return ;

}
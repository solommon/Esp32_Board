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
#include "lvgl/lvgl.h"

#define LCD_SPI_HOST    SPI2_HOST

//lcd操作句柄
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;

//刷新完成回调函数
static lcd_flush_done_cb    s_flush_done_cb = NULL;


typedef struct {
    uint8_t cmd;                //命令
    uint8_t data[8];           //数据
    uint8_t databytes;          //
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
    //{0x2A,{0,0,0,0},4,0,0},
    //{0x2B,{0,0,0,0},4,0,0},
    {0x21,{0},0,1,10},
    {0x13,{0},0,1,10},

    {0x36,{0x70},1,0,0},   //显示方向旋转

    {0x29,{0},0,1,500}
};

static bool notify_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    if(s_flush_done_cb)
        s_flush_done_cb(user_ctx);
    return false;
}


/** st7789初始化
 * @param st7789_cfg_t  接口参数
 * @return 成功或失败
*/
esp_err_t st7789_driver_hw_init(st7789_cfg_t* cfg)
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
        .sclk_io_num = cfg->clk,
        .mosi_io_num = cfg->mosi,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = cfg->width * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    s_flush_done_cb = cfg->done_cb; //设置刷新完成回调函数

    //创建基于spi的lcd操作句柄
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = cfg->dc,
        .cs_gpio_num = cfg->cs,
        .pclk_hz = cfg->spi_fre,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_flush_ready,   //刷新完成回调函数
        .user_ctx = cfg->cb_param,                                    //回调函数参数
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &lcd_io_handle));

    //命令表
    int i = 0;
    for(i = 0;i < sizeof(s_lcd_cmd_list)/sizeof(s_lcd_cmd_list[0]);i++)
    {
        esp_lcd_panel_io_tx_param(lcd_io_handle,s_lcd_cmd_list[i].cmd,s_lcd_cmd_list[i].data,s_lcd_cmd_list[i].databytes);
        if(s_lcd_cmd_list[i].delayflg)
            vTaskDelay(pdMS_TO_TICKS(s_lcd_cmd_list[i].delaytime));
    }

    return ESP_OK;
}

/** st7789写入显示数据
 * @param x1,x2,y1,y2:显示区域
 * @return 无
*/
void st7789_flush(int x1,int x2,int y1,int y2,void *data)
{
    // define an area of frame memory where MCU can access
    esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_CASET, (uint8_t[]) {
        (x1 >> 8) & 0xFF,
        x1 & 0xFF,
        ((x2 - 1) >> 8) & 0xFF,
        (x2 - 1) & 0xFF,
    }, 4);
    esp_lcd_panel_io_tx_param(lcd_io_handle, LCD_CMD_RASET, (uint8_t[]) {
        (y1 >> 8) & 0xFF,
        y1 & 0xFF,
        ((y2 - 1) >> 8) & 0xFF,
        (y2 - 1) & 0xFF,
    }, 4);
    // transfer frame buffer
    size_t len = (x2 - x1) * (y2 - y1) * 2;
    esp_lcd_panel_io_tx_color(lcd_io_handle, LCD_CMD_RAMWR, data, len);

    return ;
}

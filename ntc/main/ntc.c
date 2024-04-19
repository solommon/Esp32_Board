#include <string.h>
#include <stdio.h>
#include "ntc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define TAG     "adc"

/*
GPIO32  ADC1_CH4
GPIO33  ADC1_CH5
GPIO34  ADC1_CH6
GPIO35  ADC1_CH7
GPIO36  ADC1_CH0
GPIO37  ADC1_CH1
GPIO38  ADC1_CH2
GPIO39  ADC1_CH3
*/
#define TEMP_ADC_CHANNEL     ADC_CHANNEL_0      //ADC转换通道(ADC1有8个通道，对应gpio32 - gpio39)

#define NTC_RES             10000               //NTC电阻标称值(在电路中和NTC一起串进电路的那个电阻,一般是10K，100K)
#define ADC_V_MAX           3300                //最大接入电压值

#define ADC_VALUE_NUM       10                  //每次采样的电压

static bool do_calibration1 = false;            //是否需要校准

static volatile float s_temp_value = 0.0f;      //室内温度



static int s_adc_raw[ADC_VALUE_NUM];              //ADC采样值
static int s_voltage_raw[ADC_VALUE_NUM];              //转换后的电压值


typedef struct
{
    int8_t temp;        //温度
    uint32_t res;       //温度对应的阻值
}temp_res_t;

//NTC温度表
static const temp_res_t s_ntc_table[] =
{
    {-10,51815},
    {-9,49283},
    {-8,46889},
    {-7,44624},
    {-6,42481},
    {-5,40450},
    {-4,38526},
    {-3,36702},
    {-2,34971},
    {-1,33329},
    {0,31770},
    {1,30253},
    {2,28815},
    {3,27453},
    {4,26160},
    {5,24935},
    {6,23772},
    {7,22668},
    {8,21620},
    {9,20626},
    {10,19681},
    {11,18784},
    {12,17932},
    {13,17122},
    {14,16353},
    {15,15621},
    {16,14925},
    {17,14263},
    {18,13634},
    {19,13035},
    {20,12465},
    {21,11922},
    {22,11406},
    {23,10914},
    {24,10446},
    {25,10000},
    {26,9574},
    {27,9169},
    {28,8783},
    {29,8415},
    {30,8064},
    {31,7729},
    {32,7410},
    {33,7105},
    {34,6814},
    {35,6537},
    {36,6272},
    {37,6019},
    {38,5778},
    {39,5547},
    {40,5327},
    {41,5116},
    {42,4915},
    {43,4722},
    {44,4539},
    {45,4363},
    {46,4195},
    {47,4034},
    {48,3880},
    {49,3733},
    {50,3592},
};

//NTC表长度
static const uint16_t s_us_ntc_table_num = sizeof(s_ntc_table)/sizeof(s_ntc_table[0]);

//ADC操作句柄
static adc_oneshot_unit_handle_t s_adc_handle = NULL;

//转换句柄
static adc_cali_handle_t adc1_cali_handle = NULL;

//NTC温度采集任务
static void temp_adc_task(void*);

static float get_ntc_temp(uint32_t res);

/*---------------------------------------------------------------
        ADC校准方案，创建校准方案后，会从官方预烧录的参数中对采集到的电压进行校准
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/**
 * 温度检测初始化
 * @param 无
 * @return 无
*/
void temp_ntc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,  //WIFI和ADC2无法同时启用，这里选择ADC1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &s_adc_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,       //分辨率
        .atten = ADC_ATTEN_DB_12,          
        //衰减倍数，ESP32设计的ADC参考电压为1100mV,只能测量0-1100mV之间的电压，如果要测量更大范围的电压
        //需要设置衰减倍数
        /*以下是对应可测量范围
        ADC_ATTEN_DB_0	    100 mV ~ 950 mV
        ADC_ATTEN_DB_2_5	100 mV ~ 1250 mV
        ADC_ATTEN_DB_6	    150 mV ~ 1750 mV
        ADC_ATTEN_DB_12	    150 mV ~ 2450 mV
        */
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, TEMP_ADC_CHANNEL, &config));

    //-------------ADC1 Calibration Init---------------//
    
    do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_handle);
    xTaskCreatePinnedToCore(temp_adc_task, "adc_task", 2048, NULL,2, NULL, 1);
}

/**
 * 获取温度值
 * @param 无
 * @return 温度
*/
float get_temp(void)
{
    return s_temp_value;
}

static void temp_adc_task(void* param)
{
    uint16_t adc_cnt = 0;
    while(1)
    {
        adc_oneshot_read(s_adc_handle, TEMP_ADC_CHANNEL, &s_adc_raw[adc_cnt]);
        if (do_calibration1) {
            adc_cali_raw_to_voltage(adc1_cali_handle, s_adc_raw[adc_cnt], &s_voltage_raw[adc_cnt]);
        }
        adc_cnt++;
        if(adc_cnt >= 10)
        {
            int i = 0;
            //用平均值进行滤波
            uint32_t voltage = 0;
            uint32_t res = 0;
            for(i = 0;i < ADC_VALUE_NUM;i++)
            {
                voltage += s_voltage_raw[i];
            }
            voltage = voltage/ADC_VALUE_NUM;

            if(voltage < ADC_V_MAX)
            {
                //电压转换为相应的电阻值
                //res = (ADC_V_MAX*NTC_RES - voltage*NTC_RES)/voltage;
                res = (voltage*NTC_RES)/(ADC_V_MAX-voltage);
                //根据电阻值查表找出对应的温度
                s_temp_value = get_ntc_temp(res);
            }

            adc_cnt = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

//线性插值
static float linera_interpolation(int32_t x,int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
    float k = (float)(y2 - y1) /(float)(x2 - x1);
    float b = (float)(x2 * y1 - x1 * y2) / (float)(x2 - x1);
    float y = k * x + b;
    return y;
}

//二分查找
static int find_ntc_index(uint32_t  res,uint16_t left, uint16_t right)
{
    uint16_t middle = (left + right) / 2;
    if (right <= left || left == middle)
    {
        if (res >= s_ntc_table[left].res)
            return left;
        else
            return right;
    }
    if (res > s_ntc_table[middle].res)
    {
        right = middle;
        return find_ntc_index(res,left, right);
    }
    else if (res < s_ntc_table[middle].res)
    {
        left = middle;
        return find_ntc_index(res,left, right);
    }
    else
        return middle;
}

//根据电阻值、NTC表查出对应温度
static float get_ntc_temp(uint32_t res)
{
    uint16_t left = 0;
    uint16_t right = s_us_ntc_table_num - 1;
    int index = find_ntc_index(res,left,right);
    if (res == s_ntc_table[index].res)
        return s_ntc_table[index].temp;
    if (index == 0 || index == s_us_ntc_table_num - 1)
    {
        return s_ntc_table[index].temp;
    }
    else
    {
        int next_index = index + 1;
        return linera_interpolation(res, s_ntc_table[index].res, s_ntc_table[next_index].res, 
        s_ntc_table[index].temp, s_ntc_table[next_index].temp);
    }
}

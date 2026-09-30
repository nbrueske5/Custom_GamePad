#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "temp_sensor.h"

const static char *TAG = "temp_sensor";

adc_cali_handle_t adc1_cali_chan_handle = NULL;
adc_oneshot_unit_handle_t adc1_handle = NULL;

int adc_raw = 0;
int voltage = 0;

//todo -> allow user to modify target temp and allowed offset?
float target_temp = 22.0f; // Target temperature in Celsius
float temp_offset = 1.0; // Allowed temperature offset in Celsius
float raw_avg = 0;
float avg_voltage = 0;
float curr_temp = 0;

TaskHandle_t readSensor_hdl = NULL;

bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

void readSensorTask(void* param) {
    for (;;) {
        // Average out the input
        raw_avg = 0;
        avg_voltage = 0;

        for (int i = 0; i < AVG_SAMPLES; i++) {
            adc_oneshot_read(adc1_handle, ADC1_CHAN, &adc_raw);
            adc_cali_raw_to_voltage(adc1_cali_chan_handle, adc_raw, &voltage);
            raw_avg += adc_raw;
            avg_voltage += voltage;
            vTaskDelay(pdMS_TO_TICKS(AVG_SAMPLES_SPEED_MS / AVG_SAMPLES));
        }
        raw_avg = raw_avg/AVG_SAMPLES;
        avg_voltage = avg_voltage/AVG_SAMPLES;
        
        ESP_LOGI(TAG, "Raw AVERAGE: %.2f", raw_avg);
        ESP_LOGI(TAG, "VOLTAGE: %.2f", avg_voltage);
        curr_temp = (avg_voltage - 500) / 10.0; // For TMP36 sensor
        ESP_LOGI(TAG, "Temp: %.2f C", curr_temp); // For TMP36 sensor  
    }
    
}

void createReadSensorTask() {
    xTaskCreate (
        readSensorTask,
        "Read Sensor",
        5000,
        NULL,
        10,
        &readSensor_hdl
    );
}

void init_temp_sensor(void)
{
    float curr_temp = target_temp;
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN, &config));

    //-------------ADC1 Calibration Init---------------//
    adc_calibration_init(ADC_UNIT_1, ADC1_CHAN, ADC_ATTEN, &adc1_cali_chan_handle);
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

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
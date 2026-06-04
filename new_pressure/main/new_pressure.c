#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

#define ADC_CHANNEL ADC_CHANNEL_6   // GPIO34

#define ADC_REF_VOLTAGE        3.3f

// Divider values
#define R_TOP                  5600.0f
#define R_BOTTOM               15000.0f

// Sensor characterization
#define ZERO_PRESSURE_VOLTAGE  0.40f
#define FULL_SCALE_VOLTAGE     4.50f
#define MAX_PRESSURE_PSI       200.0f

static adc_oneshot_unit_handle_t adc_handle;

float read_adc_voltage(void)
{
    int raw;

    ESP_ERROR_CHECK(
        adc_oneshot_read(
            adc_handle,
            ADC_CHANNEL,
            &raw));

    return ((float)raw / 4095.0f) * ADC_REF_VOLTAGE;
}

float adc_to_sensor_voltage(float adc_voltage)
{
    return adc_voltage *
           ((R_TOP + R_BOTTOM) / R_BOTTOM);
}

float sensor_voltage_to_pressure(float sensor_voltage)
{
    float pressure =
        (sensor_voltage - ZERO_PRESSURE_VOLTAGE) *
        MAX_PRESSURE_PSI /
        (FULL_SCALE_VOLTAGE - ZERO_PRESSURE_VOLTAGE);

    if (pressure < 0.0f)
        pressure = 0.0f;

    return pressure;
}

void app_main(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &init_cfg,
            &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            ADC_CHANNEL,
            &chan_cfg));

    while (1)
    {
        int raw;

        ESP_ERROR_CHECK(
            adc_oneshot_read(
                adc_handle,
                ADC_CHANNEL,
                &raw));

        float adc_voltage =
            ((float)raw / 4095.0f) * ADC_REF_VOLTAGE;

        float sensor_voltage =
            adc_to_sensor_voltage(adc_voltage);

        float pressure =
            sensor_voltage_to_pressure(sensor_voltage);

        printf(
            "Raw: %4d | ADC: %.3f V | Sensor: %.3f V | Pressure: %.2f PSI\n",
            raw,
            adc_voltage,
            sensor_voltage,
            pressure);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
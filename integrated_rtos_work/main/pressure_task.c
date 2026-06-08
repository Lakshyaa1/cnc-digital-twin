#include "cnc_firmware_rtos.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "PRESSURE";

#define BMP280_I2C_PORT             I2C_NUM_0
#define BMP280_I2C_SDA              GPIO_NUM_21
#define BMP280_I2C_SCL              GPIO_NUM_22
#define BMP280_I2C_FREQ             100000
#define BMP280_I2C_ADDR             0x76
#define BMP280_PRESS_MSB            0xF7
#define BMP280_CTRL_MEAS            0xF4

#define ANALOG_PRESSURE_ADC_CHAN    ADC_CHANNEL_6
#define ADC_REF_VOLTAGE             3.3f
#define R_TOP                       5600.0f
#define R_BOTTOM                    15000.0f
#define ZERO_PRESSURE_VOLTAGE       0.40f
#define FULL_SCALE_VOLTAGE          4.50f
#define MAX_PRESSURE_PSI            200.0f
#define PSI_TO_BAR                  0.0689476f

static adc_oneshot_unit_handle_t adc_handle;
typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;

    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} bmp280_calib_t;

static bmp280_calib_t bmp_cal;
static void bmp280_read_calibration(void)
{
    uint8_t reg = 0x88;
    uint8_t calib[24];

   esp_err_t err =
        i2c_master_write_read_device(
            BMP280_I2C_PORT,
            BMP280_I2C_ADDR,
            &reg,
            1,
            calib,
            sizeof(calib),
            pdMS_TO_TICKS(100));

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read BMP280 calibration data");
        return;
    }

    bmp_cal.dig_T1 = calib[0]  | (calib[1]  << 8);
    bmp_cal.dig_T2 = calib[2]  | (calib[3]  << 8);
    bmp_cal.dig_T3 = calib[4]  | (calib[5]  << 8);

    bmp_cal.dig_P1 = calib[6]  | (calib[7]  << 8);
    bmp_cal.dig_P2 = calib[8]  | (calib[9]  << 8);
    bmp_cal.dig_P3 = calib[10] | (calib[11] << 8);
    bmp_cal.dig_P4 = calib[12] | (calib[13] << 8);
    bmp_cal.dig_P5 = calib[14] | (calib[15] << 8);
    bmp_cal.dig_P6 = calib[16] | (calib[17] << 8);
    bmp_cal.dig_P7 = calib[18] | (calib[19] << 8);
    bmp_cal.dig_P8 = calib[20] | (calib[21] << 8);
    bmp_cal.dig_P9 = calib[22] | (calib[23] << 8);

    ESP_LOGI(TAG,
             "BMP280 calib loaded P1=%u T1=%u",
             bmp_cal.dig_P1,
             bmp_cal.dig_T1);
}
static bmp280_calib_t bmp_cal;
static void bmp280_i2c_init(void)
{
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BMP280_I2C_SDA,
        .scl_io_num = BMP280_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BMP280_I2C_FREQ,
    };

    ESP_ERROR_CHECK(i2c_param_config(BMP280_I2C_PORT, &config));
    ESP_ERROR_CHECK(i2c_driver_install(BMP280_I2C_PORT, config.mode, 0, 0, 0));
    ESP_LOGI(TAG, "BMP280 I2C initialized");
}

static void bmp280_config(void)
{
    uint8_t payload[2] = {BMP280_CTRL_MEAS, 0x57};
    esp_err_t err =i2c_master_write_to_device(
        BMP280_I2C_PORT, BMP280_I2C_ADDR, payload, sizeof(payload), pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure BMP280");
    } else {
        ESP_LOGI(TAG, "BMP280 configured for normal mode"); 
    }
}
static void bmp280_read(float *pressure_pa, float *temperature_c)
{
    uint8_t data[6];
    uint8_t reg = BMP280_PRESS_MSB;
    int32_t adc_p;
    int32_t adc_t;

    double var1, var2;
    double t_fine;
    double T;

    double pvar1, pvar2;
    double pressure;

    esp_err_t err = i2c_master_write_read_device(
        BMP280_I2C_PORT,
        BMP280_I2C_ADDR,
        &reg,
        1,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100));

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read BMP280 data");
        *temperature_c = 0.0f;
        *pressure_pa = 0.0f;
        return;
    }

    adc_p = ((int32_t)data[0] << 12) |
            ((int32_t)data[1] << 4)  |
            ((int32_t)data[2] >> 4);

    adc_t = ((int32_t)data[3] << 12) |
            ((int32_t)data[4] << 4)  |
            ((int32_t)data[5] >> 4);

    /* Temperature compensation */

    var1 =
        (((double)adc_t) / 16384.0 -
         ((double)bmp_cal.dig_T1) / 1024.0) *
        ((double)bmp_cal.dig_T2);

    var2 =
        ((((double)adc_t) / 131072.0 -
          ((double)bmp_cal.dig_T1) / 8192.0) *
         (((double)adc_t) / 131072.0 -
          ((double)bmp_cal.dig_T1) / 8192.0)) *
        ((double)bmp_cal.dig_T3);

    t_fine = var1 + var2;

    T = t_fine / 5120.0;

    /* Pressure compensation */

    pvar1 = (t_fine / 2.0) - 64000.0;

    pvar2 =
        pvar1 * pvar1 *
        ((double)bmp_cal.dig_P6) / 32768.0;

    pvar2 =
        pvar2 +
        pvar1 * ((double)bmp_cal.dig_P5) * 2.0;

    pvar2 =
        (pvar2 / 4.0) +
        (((double)bmp_cal.dig_P4) * 65536.0);

    pvar1 =
        (
            (((double)bmp_cal.dig_P3) *
             pvar1 * pvar1 / 524288.0)
            +
            (((double)bmp_cal.dig_P2) *
             pvar1)
        ) / 524288.0;

    pvar1 =
        (1.0 + pvar1 / 32768.0) *
        ((double)bmp_cal.dig_P1);

    if (pvar1 == 0.0) {
        *temperature_c = (float)T;
        *pressure_pa = 0.0f;
        return;
    }

    pressure = 1048576.0 - (double)adc_p;

    pressure =
        (pressure - (pvar2 / 4096.0)) *
        6250.0 / pvar1;

    pvar1 =
        ((double)bmp_cal.dig_P9) *
        pressure * pressure /
        2147483648.0;

    pvar2 =
        pressure *
        ((double)bmp_cal.dig_P8) /
        32768.0;

    pressure =
        pressure +
        (pvar1 + pvar2 +
         ((double)bmp_cal.dig_P7)) / 16.0;

    *temperature_c = (float)T;
    *pressure_pa = (float)pressure;
}

static void analog_pressure_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ANALOG_PRESSURE_ADC_CHAN, &channel_config));
    ESP_LOGI(TAG, "Analog pressure ADC initialized");
}

static float analog_pressure_read_bar(void)
{
    int raw = 0;
    float adc_voltage;
    float sensor_voltage;
    float pressure_psi;

    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ANALOG_PRESSURE_ADC_CHAN, &raw));
    adc_voltage = ((float)raw / 4095.0f) * ADC_REF_VOLTAGE;
    sensor_voltage = adc_voltage * ((R_TOP + R_BOTTOM) / R_BOTTOM);
    pressure_psi = (sensor_voltage - ZERO_PRESSURE_VOLTAGE) * MAX_PRESSURE_PSI /
                   (FULL_SCALE_VOLTAGE - ZERO_PRESSURE_VOLTAGE);

    if (pressure_psi < 0.0f) {
        pressure_psi = 0.0f;
    }

    return pressure_psi * PSI_TO_BAR;
}

void pressure_task(void *pvParameters)
{
    (void)pvParameters;

    pressure_sample_t sample = {0};
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(500);
    uint32_t sample_count = 0;

    bmp280_i2c_init();
    bmp280_config();
    bmp280_read_calibration();
    analog_pressure_init();

    ESP_LOGI(TAG, "pressure_task running in polling mode at 2 Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        bmp280_read(&sample.bmp280.pressure_pa, &sample.bmp280.temperature_c);
        sample.bmp280.timestamp_ms = get_timestamp_ms();

        sample.analog.analog_pressure_bar = analog_pressure_read_bar();
        sample.analog.timestamp_ms = get_timestamp_ms();
        sample.timestamp_ms = sample.analog.timestamp_ms;

        if (xQueueSend(pressure_queue, &sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "pressure_queue full, sample dropped");
        }

        sample_count++;
        if ((sample_count % 4U) == 0U) {
            ESP_LOGD(TAG, "pressure: bmp280=%.2f Pa temp=%.2f C analog=%.2f bar",
                     sample.bmp280.pressure_pa,
                     sample.bmp280.temperature_c,
                     sample.analog.analog_pressure_bar);
        }
    }
}

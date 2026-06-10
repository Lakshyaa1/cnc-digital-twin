#include "cnc_firmware_rtos.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "AGGREGATOR";

#define KX134_COUNTS_PER_G   4096.0f
#define VIBRATION_FILTER_SIZE 5

static float vibration_history[VIBRATION_FILTER_SIZE] = {0};
static uint8_t vibration_index = 0;
void aggregator_task(void *pvParameters)
{
    accel_sample_t accel_sample = {0};
    pressure_sample_t pressure_sample = {0};
    ultrasonic_sample_t ultrasonic_sample = {0};
    water_level_sample_t water_sample = {0};
    barcode_sample_t barcode_sample = {0};
    rfid_sample_t rfid_sample = {0};
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100);
    uint32_t cycle_count = 0;

    (void)pvParameters;

    ESP_LOGI(TAG, "aggregator_task running at 10 Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        while (xQueueReceive(accel_queue, &accel_sample, 0) == pdTRUE) {}
        while (xQueueReceive(pressure_queue, &pressure_sample, 0) == pdTRUE) {}
        while (xQueueReceive(ultrasonic_queue, &ultrasonic_sample, 0) == pdTRUE) {}
        while (xQueueReceive(housekeep_queue, &water_sample, 0) == pdTRUE) {}
        while (xQueueReceive(barcode_queue, &barcode_sample, 0) == pdTRUE) {}
        while (xQueueReceive(rfid_queue, &rfid_sample, 0) == pdTRUE) {}

        float vibration_g =
            sqrtf(
                (float)accel_sample.x * accel_sample.x +
                (float)accel_sample.y * accel_sample.y +
                (float)accel_sample.z * accel_sample.z
            ) / KX134_COUNTS_PER_G;

        vibration_history[vibration_index] = vibration_g;

        vibration_index =
            (vibration_index + 1) % VIBRATION_FILTER_SIZE;

        float filtered_vibration_g = 0.0f;

        for (int i = 0; i < VIBRATION_FILTER_SIZE; i++)
        {
            filtered_vibration_g += vibration_history[i];
        }

        filtered_vibration_g /= VIBRATION_FILTER_SIZE;

        sensor_data_lock();
        shared_sensor_data.last_accel = accel_sample;

        shared_sensor_data.vibration_g = vibration_g;
        shared_sensor_data.filtered_vibration_g = filtered_vibration_g;
        shared_sensor_data.last_bmp280 = pressure_sample.bmp280;
        shared_sensor_data.last_analog_pressure = pressure_sample.analog;
        shared_sensor_data.last_ultrasonic = ultrasonic_sample;
        shared_sensor_data.last_water_level = water_sample;

        if (barcode_sample.timestamp_ms != 0) {
            shared_sensor_data.last_barcode = barcode_sample;
        }
        if (rfid_sample.timestamp_ms != 0) {
            shared_sensor_data.last_rfid = rfid_sample;
        }

        shared_sensor_data.accel_anomaly =
            (filtered_vibration_g > 2.5f) ? 1U : 0U;
        shared_sensor_data.pressure_anomaly =
            (pressure_sample.bmp280.pressure_pa < 80000.0f ||
             pressure_sample.bmp280.pressure_pa > 120000.0f) ? 1U : 0U;
        shared_sensor_data.temp_anomaly =
            (pressure_sample.bmp280.temperature_c < 15.0f ||
             pressure_sample.bmp280.temperature_c > 45.0f) ? 1U : 0U;
        shared_sensor_data.last_update_ms = get_timestamp_ms();
        sensor_data_unlock();

        cycle_count++;
        if ((cycle_count % 10U) == 0U) {
            ESP_LOGI(
                TAG,
                "snapshot: accel=(%.3f,%.3f,%.3f) g vib=%.3fg filtered=%.3fg bmp280=%.2f Pa temp=%.2f C gauge=%.2f bar dist=%.2f cm water=%s barcode=%s rfid=%s auth=%s",
                accel_sample.x / KX134_COUNTS_PER_G,
                accel_sample.y / KX134_COUNTS_PER_G,
                accel_sample.z / KX134_COUNTS_PER_G,
                vibration_g,
                filtered_vibration_g,
                pressure_sample.bmp280.pressure_pa,
                pressure_sample.bmp280.temperature_c,
                pressure_sample.analog.analog_pressure_bar,
                ultrasonic_sample.distance_cm,
                water_sample.water_detected ? "DETECTED" : "CLEAR",
                barcode_sample.timestamp_ms ? barcode_sample.barcode : "NONE",
                rfid_sample.timestamp_ms ? rfid_sample.rfid_uid : "NONE",
                shared_sensor_data.user_authenticated ? "PASS" : "FAIL");
        }
    }
}

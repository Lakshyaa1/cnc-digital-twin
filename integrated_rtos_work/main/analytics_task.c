#include "cnc_firmware_rtos.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "ANALYTICS";

#define KX134_COUNTS_PER_G         4096.0f

// ===== ANOMALY DETECTION THRESHOLDS =====
#define VIBRATION_THRESHOLD_G       5.0f    // 5g for alarm
#define PRESSURE_MIN_KPA           80.0f
#define PRESSURE_MAX_KPA           120.0f
#define TEMP_MIN_C                 15.0f
#define TEMP_MAX_C                 45.0f
#define DISTANCE_COLLISION_CM      20.0f   // Collision if < 20cm

// ===== ANALYTICS TASK (LOW PRIORITY, 2Hz) =====
// Reads from shared data and performs predictive maintenance analysis
void analytics_task(void *pvParameters) {
    accel_sample_t accel_copy = {0};
    bmp280_sample_t pressure_copy = {0};
    ultrasonic_sample_t distance_copy = {0};

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(500);  // 500ms = 2Hz analysis
    uint32_t analysis_count = 0;

    ESP_LOGI(TAG, "analytics_task running at 2Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        // Read shared data (protected by mutex)
        sensor_data_lock();
        {
            memcpy(&accel_copy, &shared_sensor_data.last_accel, sizeof(accel_sample_t));
            memcpy(&pressure_copy, &shared_sensor_data.last_bmp280, sizeof(bmp280_sample_t));
            memcpy(&distance_copy, &shared_sensor_data.last_ultrasonic, sizeof(ultrasonic_sample_t));
        }
        sensor_data_unlock();

        // ===== VIBRATION ANALYSIS =====
        float accel_mag = sqrtf(
            (float)accel_copy.x * accel_copy.x +
            (float)accel_copy.y * accel_copy.y +
            (float)accel_copy.z * accel_copy.z
        );
        float accel_g = accel_mag / KX134_COUNTS_PER_G;

        if (accel_g > VIBRATION_THRESHOLD_G) {
            ESP_LOGW(TAG, "HIGH VIBRATION ALERT: %.2f g (threshold: %.2f g)", 
                     accel_g, VIBRATION_THRESHOLD_G);
            // TODO: Log to predictive maintenance database
            // Possible causes: bearing wear, spindle imbalance, tool breakage
        }

        // ===== PRESSURE ANALYSIS =====
        float pressure_kpa = pressure_copy.pressure_pa / 1000.0f;

        if (pressure_kpa < PRESSURE_MIN_KPA) {
            ESP_LOGW(TAG, "LOW PRESSURE ALERT: %.1f kPa (min: %.1f kPa)", 
                     pressure_kpa, PRESSURE_MIN_KPA);
            // TODO: Check coolant level, pump failure
        } else if (pressure_kpa > PRESSURE_MAX_KPA) {
            ESP_LOGW(TAG, "HIGH PRESSURE ALERT: %.1f kPa (max: %.1f kPa)", 
                     pressure_kpa, PRESSURE_MAX_KPA);
            // TODO: Check for blockage, regulator failure
        }

        // ===== TEMPERATURE ANALYSIS =====
        if (pressure_copy.temperature_c < TEMP_MIN_C) {
            ESP_LOGW(TAG, "LOW TEMPERATURE ALERT: %.1f°C (min: %.1f°C)", 
                     pressure_copy.temperature_c, TEMP_MIN_C);
            // TODO: Check coolant viscosity, spindle bearing temp
        } else if (pressure_copy.temperature_c > TEMP_MAX_C) {
            ESP_LOGW(TAG, "HIGH TEMPERATURE ALERT: %.1f°C (max: %.1f°C)", 
                     pressure_copy.temperature_c, TEMP_MAX_C);
            // TODO: Check for overheating, poor cooling
        }

        // ===== COLLISION DETECTION =====
        if (distance_copy.distance_cm > 0 && distance_copy.distance_cm < DISTANCE_COLLISION_CM) {
            ESP_LOGE(TAG, "COLLISION WARNING: %.1f cm (threshold: %.1f cm)",
                     distance_copy.distance_cm, DISTANCE_COLLISION_CM);
            // TODO: Trigger emergency stop
        }

        analysis_count++;
        if (analysis_count % 10 == 0) {
            ESP_LOGD(TAG, "Analysis cycle #%lu: accel=%.2fg, pressure=%.1fkPa, temp=%.1f°C, dist=%.1fcm",
                     analysis_count, accel_g, pressure_kpa, 
                     pressure_copy.temperature_c, distance_copy.distance_cm);
        }
    }
}

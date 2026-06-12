#include "cnc_firmware_rtos.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "ANALYTICS";
static uint8_t calculate_health_score(
                                      /*
                                       * CNC Machine Health Score (0-100)
                                       *
                                       * Purpose:
                                       * Combines multiple sensor readings into a single machine-health metric
                                       * for predictive maintenance and dashboard visualization.
                                       *
                                       * Score starts at 100 and penalties are applied based on abnormal
                                       * operating conditions.
                                       *
                                       * Vibration RMS:
                                       *   > 1.5g  -> -10 points
                                       *   > 2.0g  -> -20 points
                                       *   > 3.0g  -> -40 points
                                       *
                                       * Temperature:
                                       *   > 45°C  -> -15 points
                                       *   > 55°C  -> -30 points
                                       *
                                       * Pressure:
                                       *   Outside 80-120 kPa -> -20 points
                                       *
                                       * Water Detection:
                                       *   Water detected -> -30 points
                                       *
                                       * Collision Risk:
                                       *   Distance < 20 cm -> -20 points
                                       *
                                       * Health Categories:
                                       *   80-100 : Healthy
                                       *   60-79  : Warning
                                       *   40-59  : Maintenance Required
                                       *   0-39   : Critical
                                       *
                                       * Future Improvements:
                                       *   - Historical trend analysis
                                       *   - FFT-based vibration penalties
                                       *   - Bearing wear prediction
                                       *   - Remaining Useful Life (RUL) estimation
                                       */
    float vibration_rms_g,
    float temperature_c,
    float pressure_pa,
    float distance_cm,
    bool water_detected)
{
    int score = 100;

    /* RMS vibration */
    if (vibration_rms_g > 3.0f)
        score -= 40;
    else if (vibration_rms_g > 2.0f)
        score -= 20;
    else if (vibration_rms_g > 1.5f)
        score -= 10;

    /* Temperature */
    if (temperature_c > 55.0f)
        score -= 30;
    else if (temperature_c > 45.0f)
        score -= 15;

    /* Pressure */
    if (pressure_pa < 80000.0f ||
        pressure_pa > 120000.0f)
        score -= 20;

    /* Water */
    if (water_detected)
        score -= 30;

    /* Collision risk */
    if (distance_cm > 0 &&
        distance_cm < 20.0f)
        score -= 20;

    if (score < 0)
        score = 0;

    return (uint8_t)score;
}
#define KX134_COUNTS_PER_G         4096.0f

// ===== ANOMALY DETECTION THRESHOLDS =====
#define VIBRATION_THRESHOLD_G       2.5f    // 5g for alarm
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

    float filtered_vibration_g = 0.0f;
    float vibration_rms_g = 0.0f;
    uint8_t health_score = 100;
    bool water_detected = false;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(500);  // 500ms = 2Hz analysis
    uint32_t analysis_count = 0;

    ESP_LOGI(TAG, "analytics_task running at 2Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        // Read shared data (protected by mutex)
        sensor_data_lock();
        {
            memcpy(&accel_copy,
                   &shared_sensor_data.last_accel,
                   sizeof(accel_sample_t));

            filtered_vibration_g =
                shared_sensor_data.filtered_vibration_g;
            vibration_rms_g =
                shared_sensor_data.vibration_rms_g;

            water_detected =
                shared_sensor_data.last_water_level.water_detected;

            memcpy(&pressure_copy,
                   &shared_sensor_data.last_bmp280,
                   sizeof(bmp280_sample_t));
            memcpy(&distance_copy, &shared_sensor_data.last_ultrasonic, sizeof(ultrasonic_sample_t));
        }
        sensor_data_unlock();

        // ===== VIBRATION ANALYSIS =====
        float accel_g = filtered_vibration_g;

        if (accel_g > VIBRATION_THRESHOLD_G) {
            ESP_LOGW(TAG,
                     "FILTERED VIBRATION ALERT: %.2f g (threshold: %.2f g)",
                     accel_g,
                     VIBRATION_THRESHOLD_G);
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
        health_score =
            calculate_health_score(
                vibration_rms_g,
                pressure_copy.temperature_c,
                pressure_copy.pressure_pa,
                distance_copy.distance_cm,
                water_detected);

        sensor_data_lock();

        shared_sensor_data.health_score =
            health_score;

        if (health_score >= 80)
        {
            strcpy(shared_sensor_data.machine_status,
                   "EXCELLENT");
        }
        else if (health_score >= 60)
        {
            strcpy(shared_sensor_data.machine_status,
                   "HEALTHY");
        }
        else if (health_score >= 40)
        {
            strcpy(shared_sensor_data.machine_status,
                   "WARNING");
        }
        else if (health_score >= 20)
        {
            strcpy(shared_sensor_data.machine_status,
                   "MAINTENANCE");
        }
        else
        {
            strcpy(shared_sensor_data.machine_status,
                   "CRITICAL");
        }

        sensor_data_unlock();
        analysis_count++;
        if (analysis_count % 10 == 0) {
            ESP_LOGI(
                TAG,
                "Health=%u RMS=%.2fg Temp=%.1fC Pressure=%.1fkPa Dist=%.1fcm",
                health_score,
                vibration_rms_g,
                pressure_copy.temperature_c,
                pressure_kpa,
                distance_copy.distance_cm);
        }
    }
}

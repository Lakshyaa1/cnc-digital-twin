#ifndef CNC_FIRMWARE_RTOS_H
#define CNC_FIRMWARE_RTOS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <time.h>

// ===== TASK PRIORITIES =====
// Higher number = higher priority
#define ACCEL_TASK_PRIORITY           (configMAX_PRIORITIES - 1)  // Highest - vibration critical
#define PRESSURE_TASK_PRIORITY        (configMAX_PRIORITIES - 2)  // High - system safety
#define ULTRASONIC_TASK_PRIORITY      (configMAX_PRIORITIES - 2)  // High - collision safety
#define HOUSEKEEPING_TASK_PRIORITY    (configMAX_PRIORITIES - 4)  // Low - temp + water level
#define BARCODE_HANDLER_PRIORITY      (configMAX_PRIORITIES - 3)  // Medium - user-facing
#define RFID_HANDLER_PRIORITY         (configMAX_PRIORITIES - 3)  // Medium - user-facing
#define AGGREGATOR_TASK_PRIORITY      (configMAX_PRIORITIES - 4)  // Low - shuffles data
#define ANALYTICS_TASK_PRIORITY       (configMAX_PRIORITIES - 4)  // Low - future server push

// ===== TASK STACK SIZES =====
#define ACCEL_TASK_STACK_SIZE         4096
#define PRESSURE_TASK_STACK_SIZE      3072
#define ULTRASONIC_TASK_STACK_SIZE    3072
#define HOUSEKEEPING_TASK_STACK_SIZE  2048
#define BARCODE_HANDLER_STACK_SIZE    4096   // FFT + DB lookup
#define RFID_HANDLER_STACK_SIZE       3072
#define AGGREGATOR_TASK_STACK_SIZE    4096
#define ANALYTICS_TASK_STACK_SIZE     3072

// ===== QUEUE SIZES =====
#define ACCEL_QUEUE_SIZE              100    // 1kHz × 100ms = 100 samples buffer
#define PRESSURE_QUEUE_SIZE           50
#define ULTRASONIC_QUEUE_SIZE         50
#define HOUSEKEEP_QUEUE_SIZE          20
#define BARCODE_QUEUE_SIZE            5
#define RFID_QUEUE_SIZE               5

// ===== SENSOR DATA TYPES =====
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    uint64_t timestamp_ms;
} accel_sample_t;

typedef struct {
    float pressure_pa;
    float temperature_c;
    uint64_t timestamp_ms;
} bmp280_sample_t;

typedef struct {
    float analog_pressure_bar;
    uint64_t timestamp_ms;
} analog_pressure_sample_t;

typedef struct {
    bmp280_sample_t bmp280;
    analog_pressure_sample_t analog;
    uint64_t timestamp_ms;
} pressure_sample_t;

typedef struct {
    float distance_cm;
    uint64_t timestamp_ms;
} ultrasonic_sample_t;

typedef struct {
    uint8_t water_detected;  // 0 or 1
    uint64_t timestamp_ms;
} water_level_sample_t;

typedef struct {
    char barcode[128];
    uint64_t timestamp_ms;
} barcode_sample_t;

typedef struct {
    char rfid_uid[20];
    uint8_t uid_length;
    uint64_t timestamp_ms;
} rfid_sample_t;

// ===== SHARED SENSOR DATA STRUCT (MUTEX PROTECTED) =====
typedef struct {
    // Latest readings
    accel_sample_t last_accel;
    bmp280_sample_t last_bmp280;
    analog_pressure_sample_t last_analog_pressure;
    ultrasonic_sample_t last_ultrasonic;
    water_level_sample_t last_water_level;
    barcode_sample_t last_barcode;
    rfid_sample_t last_rfid;

    // Vibration metrics
    float vibration_g;
    float filtered_vibration_g;
    float vibration_rms_g;
    uint8_t health_score;
    char machine_status[20];

    // Anomaly flags
    uint8_t accel_anomaly;
    uint8_t pressure_anomaly;
    uint8_t temp_anomaly;
    uint8_t user_authenticated;

    // Timestamp of last update
    uint64_t last_update_ms;
} sensor_data_t;

// ===== GLOBAL HANDLES (extern in .c files) =====
extern QueueHandle_t accel_queue;
extern QueueHandle_t pressure_queue;
extern QueueHandle_t ultrasonic_queue;
extern QueueHandle_t housekeep_queue;
extern QueueHandle_t barcode_queue;
extern QueueHandle_t rfid_queue;

extern SemaphoreHandle_t barcode_sem;
extern SemaphoreHandle_t rfid_sem;

extern SemaphoreHandle_t sensor_data_mutex;
extern sensor_data_t shared_sensor_data;

// ===== FUNCTION DECLARATIONS =====
void rtos_init(void);
void accel_task(void *pvParameters);
void pressure_task(void *pvParameters);
void ultrasonic_task(void *pvParameters);
void housekeeping_task(void *pvParameters);
void barcode_handler_task(void *pvParameters);
void rfid_handler_task(void *pvParameters);
void aggregator_task(void *pvParameters);
void analytics_task(void *pvParameters);
void mqtt_task(void *pvParameters);
// Utility functions
uint64_t get_timestamp_ms(void);
void sensor_data_lock(void);
void sensor_data_unlock(void);

#endif

#include "cnc_firmware_rtos.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include "mqtt_task.h"

static const char *TAG = "CNC_RTOS";

// ===== GLOBAL QUEUE HANDLES =====
QueueHandle_t accel_queue = NULL;
QueueHandle_t pressure_queue = NULL;
QueueHandle_t ultrasonic_queue = NULL;
QueueHandle_t housekeep_queue = NULL;
QueueHandle_t barcode_queue = NULL;
QueueHandle_t rfid_queue = NULL;

// ===== GLOBAL SEMAPHORE HANDLES =====
SemaphoreHandle_t barcode_sem = NULL;
SemaphoreHandle_t rfid_sem = NULL;

// ===== GLOBAL MUTEX + SHARED DATA =====
SemaphoreHandle_t sensor_data_mutex = NULL;
sensor_data_t shared_sensor_data = {0};

// ===== UTILITY: TIMESTAMP =====
uint64_t get_timestamp_ms(void) {
    return esp_timer_get_time() / 1000ULL;
}

// ===== UTILITY: SENSOR DATA LOCKING =====
void sensor_data_lock(void) {
    xSemaphoreTake(sensor_data_mutex, portMAX_DELAY);
}

void sensor_data_unlock(void) {
    xSemaphoreGive(sensor_data_mutex);
}

// ===== INITIALIZATION =====
void rtos_init(void) {
    ESP_LOGI(TAG, "Initializing RTOS architecture...");

    // Create queues
    accel_queue = xQueueCreate(ACCEL_QUEUE_SIZE, sizeof(accel_sample_t));
    pressure_queue = xQueueCreate(PRESSURE_QUEUE_SIZE, sizeof(pressure_sample_t));
    ultrasonic_queue = xQueueCreate(ULTRASONIC_QUEUE_SIZE, sizeof(ultrasonic_sample_t));
    housekeep_queue = xQueueCreate(HOUSEKEEP_QUEUE_SIZE, sizeof(water_level_sample_t));
    barcode_queue = xQueueCreate(BARCODE_QUEUE_SIZE, sizeof(barcode_sample_t));
    rfid_queue = xQueueCreate(RFID_QUEUE_SIZE, sizeof(rfid_sample_t));

    if (!accel_queue || !pressure_queue || !ultrasonic_queue || 
        !housekeep_queue || !barcode_queue || !rfid_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }
    ESP_LOGI(TAG, "✓ Queues created");

    // Create semaphores
    barcode_sem = xSemaphoreCreateBinary();
    rfid_sem = xSemaphoreCreateBinary();

    if (!barcode_sem || !rfid_sem) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        return;
    }
    ESP_LOGI(TAG, "✓ Semaphores created");

    // Create mutex
    sensor_data_mutex = xSemaphoreCreateMutex();
    if (!sensor_data_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    ESP_LOGI(TAG, "✓ Mutex created");
 
    // Initialize shared data structure
    memset(&shared_sensor_data, 0, sizeof(sensor_data_t));
    shared_sensor_data.last_update_ms = get_timestamp_ms();

    // Create tasks
    BaseType_t ret;

    ret = xTaskCreate(accel_task, "accel_task", ACCEL_TASK_STACK_SIZE, 
                      NULL, ACCEL_TASK_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create accel_task");
    else ESP_LOGI(TAG, "✓ accel_task created (priority %d)", ACCEL_TASK_PRIORITY);

    ret = xTaskCreate(pressure_task, "pressure_task", PRESSURE_TASK_STACK_SIZE,
                      NULL, PRESSURE_TASK_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create pressure_task");
    else ESP_LOGI(TAG, "✓ pressure_task created (priority %d)", PRESSURE_TASK_PRIORITY);

    
    ret = xTaskCreate(ultrasonic_task, "ultrasonic_task", ULTRASONIC_TASK_STACK_SIZE,
                      NULL, ULTRASONIC_TASK_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create ultrasonic_task");
    else ESP_LOGI(TAG, "✓ ultrasonic_task created (priority %d)", ULTRASONIC_TASK_PRIORITY);
   
    ret = xTaskCreate(housekeeping_task, "housekeeping_task", HOUSEKEEPING_TASK_STACK_SIZE,
                      NULL, HOUSEKEEPING_TASK_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create housekeeping_task");
    else ESP_LOGI(TAG, "✓ housekeeping_task created (priority %d)", HOUSEKEEPING_TASK_PRIORITY);

    ret = xTaskCreate(barcode_handler_task, "barcode_handler", BARCODE_HANDLER_STACK_SIZE,
                      NULL, BARCODE_HANDLER_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create barcode_handler_task");
    else ESP_LOGI(TAG, "✓ barcode_handler_task created (priority %d)", BARCODE_HANDLER_PRIORITY);

    ret = xTaskCreate(rfid_handler_task, "rfid_handler", RFID_HANDLER_STACK_SIZE,
                      NULL, RFID_HANDLER_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create rfid_handler_task");
    else ESP_LOGI(TAG, "✓ rfid_handler_task created (priority %d)", RFID_HANDLER_PRIORITY);

    ret = xTaskCreate(aggregator_task, "aggregator", AGGREGATOR_TASK_STACK_SIZE,
                      NULL, AGGREGATOR_TASK_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create aggregator_task");
    else ESP_LOGI(TAG, "✓ aggregator_task created (priority %d)", AGGREGATOR_TASK_PRIORITY);

    ret = xTaskCreate(analytics_task, "analytics", ANALYTICS_TASK_STACK_SIZE,
                      NULL, ANALYTICS_TASK_PRIORITY, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create analytics_task");
    else ESP_LOGI(TAG, "✓ analytics_task created (priority %d)", ANALYTICS_TASK_PRIORITY);
    
    ret = xTaskCreate(
        mqtt_task,
        "mqtt_task",
        4096,
        NULL,
        3,
        NULL);

    if (ret != pdPASS)
        ESP_LOGE(TAG, "Failed to create mqtt_task");
    else
        ESP_LOGI(TAG, "✓ mqtt_task created");

    ESP_LOGI(TAG, "===== RTOS initialization complete =====");
    
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cnc_firmware_rtos.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "===== CNC PREDICTIVE MAINTENANCE FIRMWARE =====");
    ESP_LOGI(TAG, "Starting RTOS architecture...");

    // Initialize NVS (non-volatile storage) for WiFi/credentials later
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Start RTOS architecture
    rtos_init();

    // At this point, all tasks are running and scheduler has taken over
    // This main task can be suspended or used for high-level monitoring

    ESP_LOGI(TAG, "RTOS architecture online. Scheduler running.");
    ESP_LOGI(TAG, "All sensor tasks are active:");
    ESP_LOGI(TAG, "  - accel_task (1kHz)");
    ESP_LOGI(TAG, "  - pressure_task (10Hz)");
    ESP_LOGI(TAG, "  - ultrasonic_task (10Hz)");
    ESP_LOGI(TAG, "  - housekeeping_task (1Hz)");
    ESP_LOGI(TAG, "  - barcode_handler_task (event-driven)");
    ESP_LOGI(TAG, "  - rfid_handler_task (event-driven)");
    ESP_LOGI(TAG, "  - aggregator_task (10Hz)");
    ESP_LOGI(TAG, "  - analytics_task (2Hz)");

    while (1) {
        // Optional: Main task can monitor system health, heap, task stats, etc.
        vTaskDelay(pdMS_TO_TICKS(5000));  // Log every 5 seconds

        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_free_heap = esp_get_minimum_free_heap_size();

        ESP_LOGI(TAG, "System Health: free_heap=%lu min_free=%lu", free_heap, min_free_heap);

        // Optional: Print FreeRTOS task stats
        #if 0
        char buffer[1024];
        vTaskList(buffer);
        ESP_LOGI(TAG, "Task Stats:\n%s", buffer);
        #endif
    }
}

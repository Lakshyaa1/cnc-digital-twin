#include "cnc_firmware_rtos.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "HOUSEKEEPING";

#define WATER_SENSOR_PIN GPIO_NUM_4

static void water_sensor_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << WATER_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
}

void housekeeping_task(void *pvParameters)
{
    (void)pvParameters;

    water_level_sample_t sample = {0};
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000);

    water_sensor_init();
    ESP_LOGI(TAG, "housekeeping_task running at 1 Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        sample.water_detected = (uint8_t)gpio_get_level(WATER_SENSOR_PIN);
        sample.timestamp_ms = get_timestamp_ms();

        if (xQueueSend(housekeep_queue, &sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "housekeep_queue full, sample dropped");
        }

        ESP_LOGD(TAG, "water sensor: %s", sample.water_detected ? "detected" : "clear");
        ESP_LOGI(TAG, "heap free: %lu", esp_get_free_heap_size());
    }
}

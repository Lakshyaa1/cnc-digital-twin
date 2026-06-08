#include "cnc_firmware_rtos.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "ULTRASONIC";

#define TRIG_PIN             GPIO_NUM_13
#define ECHO_PIN             GPIO_NUM_12
#define RMT_RESOLUTION_HZ    1000000

static rmt_channel_handle_t rx_chan = NULL;
static rmt_symbol_word_t rx_symbols[4];
static TaskHandle_t ultrasonic_task_handle = NULL;

static bool rmt_rx_done_callback(
    rmt_channel_handle_t channel,
    const rmt_rx_done_event_data_t *edata,
    void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;

    (void)channel;
    (void)edata;
    (void)user_ctx;

    vTaskNotifyGiveFromISR(ultrasonic_task_handle, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static void ultrasonic_init(void)
{
    gpio_config_t trig_cfg = {
        .pin_bit_mask = (1ULL << TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = ECHO_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
    };
    rmt_rx_event_callbacks_t callbacks = {
        .on_recv_done = rmt_rx_done_callback,
    };

    ESP_ERROR_CHECK(gpio_config(&trig_cfg));
    gpio_set_level(TRIG_PIN, 0);

    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_cfg, &rx_chan));
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_chan, &callbacks, NULL));
    ESP_ERROR_CHECK(rmt_enable(rx_chan));
}

static void trigger_sensor(void)
{
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);
}

static float ultrasonic_get_distance_cm(void)
{
    rmt_receive_config_t receive_cfg = {
        .signal_range_min_ns = 1000,
        .signal_range_max_ns = 30000000,
    };
    uint32_t pulse_width_us = 0;
    rmt_symbol_word_t symbol;

    ESP_ERROR_CHECK(rmt_receive(rx_chan, rx_symbols, sizeof(rx_symbols), &receive_cfg));
    trigger_sensor();

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == 0) {
        ESP_LOGW(TAG, "RMT timeout");
        return -1.0f;
    }

    symbol = rx_symbols[0];
    if (symbol.level0 == 1) {
        pulse_width_us = symbol.duration0;
    } else if (symbol.level1 == 1) {
        pulse_width_us = symbol.duration1;
    } else {
        ESP_LOGW(TAG, "No echo pulse captured");
        return -1.0f;
    }

    if (pulse_width_us < 100 || pulse_width_us > 25000) {
        ESP_LOGW(TAG, "Invalid pulse width: %lu us", (unsigned long)pulse_width_us);
        return -1.0f;
    }

    return (pulse_width_us * 0.0343f) / 2.0f;
}

void ultrasonic_task(void *pvParameters)
{
    (void)pvParameters;

    ultrasonic_sample_t sample = {0};
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(500);
    uint32_t sample_count = 0;

    ultrasonic_task_handle = xTaskGetCurrentTaskHandle();
    ultrasonic_init();

    ESP_LOGI(TAG, "ultrasonic_task running in polling mode at 2 Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        sample.distance_cm = ultrasonic_get_distance_cm();
        sample.timestamp_ms = get_timestamp_ms();

        if (xQueueSend(ultrasonic_queue, &sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "ultrasonic_queue full, sample dropped");
        }

        sample_count++;
        if ((sample_count % 2U) == 0U) {
            ESP_LOGD(TAG, "distance=%.2f cm", sample.distance_cm);
        }
    }
}

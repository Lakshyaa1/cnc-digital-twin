#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/rmt_rx.h"

#include "esp_rom_sys.h"
#include "esp_err.h"

#define TRIG_PIN GPIO_NUM_5
#define ECHO_PIN GPIO_NUM_18

#define RMT_RESOLUTION_HZ 1000000

static rmt_channel_handle_t rx_chan = NULL;
static rmt_symbol_word_t rx_symbols[4];

static TaskHandle_t ultrasonic_task_handle = NULL;

/* -------------------------------------------------- */
/* RMT RX DONE CALLBACK                               */
/* -------------------------------------------------- */

static bool rmt_rx_done_callback(
    rmt_channel_handle_t channel,
    const rmt_rx_done_event_data_t *edata,
    void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;

    vTaskNotifyGiveFromISR(
        ultrasonic_task_handle,
        &high_task_wakeup);

    return high_task_wakeup == pdTRUE;
}

/* -------------------------------------------------- */
/* INIT                                               */
/* -------------------------------------------------- */

static void ultrasonic_init(void)
{
    gpio_config_t trig_cfg = {
        .pin_bit_mask = (1ULL << TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&trig_cfg));

    gpio_set_level(TRIG_PIN, 0);

    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = ECHO_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
    };

    ESP_ERROR_CHECK(
        rmt_new_rx_channel(
            &rx_cfg,
            &rx_chan
        )
    );

    rmt_rx_event_callbacks_t callbacks = {
        .on_recv_done = rmt_rx_done_callback,
    };

    ESP_ERROR_CHECK(
        rmt_rx_register_event_callbacks(
            rx_chan,
            &callbacks,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        rmt_enable(rx_chan)
    );
}

/* -------------------------------------------------- */
/* TRIGGER                                            */
/* -------------------------------------------------- */

static void trigger_sensor(void)
{
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);

    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);

    gpio_set_level(TRIG_PIN, 0);
}

/* -------------------------------------------------- */
/* MEASURE                                            */
/* -------------------------------------------------- */

static float ultrasonic_get_distance_cm(void)
{
    rmt_receive_config_t receive_cfg = {
        .signal_range_min_ns = 1000,
        .signal_range_max_ns = 30000000,
    };

    ESP_ERROR_CHECK(
        rmt_receive(
            rx_chan,
            rx_symbols,
            sizeof(rx_symbols),
            &receive_cfg
        )
    );

    trigger_sensor();

    if (ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(100))
        == 0)
    {
        printf("RMT TIMEOUT\n");
        return -1;
    }

    rmt_symbol_word_t s = rx_symbols[0];

    printf(
        "RAW -> L0=%d D0=%u | L1=%d D1=%u\n",
        s.level0,
        s.duration0,
        s.level1,
        s.duration1
    );

    uint32_t pulse_width_us = 0;

    if (s.level0 == 1)
    {
        pulse_width_us = s.duration0;
    }
    else if (s.level1 == 1)
    {
        pulse_width_us = s.duration1;
    }
    else
    {
        printf("NO HIGH PULSE FOUND\n");
        return -1;
    }

    printf(
        "Pulse Width = %lu us\n",
        (unsigned long)pulse_width_us
    );

    if (pulse_width_us < 100 ||
        pulse_width_us > 25000)
    {
        printf(
            "INVALID PULSE WIDTH = %lu\n",
            (unsigned long)pulse_width_us
        );

        return -1;
    }

    float distance_cm =
        (pulse_width_us * 0.0343f) / 2.0f;

    return distance_cm;
}

/* -------------------------------------------------- */
/* MAIN                                               */
/* -------------------------------------------------- */

void app_main(void)
{
    ultrasonic_task_handle =
        xTaskGetCurrentTaskHandle();

    ultrasonic_init();

    while (1)
    {
        float distance =
            ultrasonic_get_distance_cm();

        if (distance > 0)
        {
            printf(
                "Distance = %.2f cm\n\n",
                distance
            );
        }
        else
        {
            printf(
                "Measurement Failed\n\n"
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(500)
        );
    }
}
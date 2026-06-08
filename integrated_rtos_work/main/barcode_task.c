#include "cnc_firmware_rtos.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BARCODE";

#define BARCODE_UART_NUM        UART_NUM_2
#define BARCODE_UART_TX_PIN     GPIO_NUM_17
#define BARCODE_UART_RX_PIN     GPIO_NUM_16
#define BARCODE_UART_BAUD       9600
#define BARCODE_BUF_SIZE        256

static int barcode_lookup_stl_file(const char *code)
{
    if (strstr(code, "PART_001") != NULL) {
        return 1;
    }
    if (strstr(code, "PART_002") != NULL) {
        return 2;
    }
    if (strstr(code, "PART_003") != NULL) {
        return 3;
    }

    return -1;
}

static void barcode_uart_init(void)
{
    uart_config_t config = {
        .baud_rate = BARCODE_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
    };

    ESP_ERROR_CHECK(uart_param_config(BARCODE_UART_NUM, &config));
    ESP_ERROR_CHECK(uart_set_pin(
        BARCODE_UART_NUM,
        BARCODE_UART_TX_PIN,
        BARCODE_UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BARCODE_UART_NUM, BARCODE_BUF_SIZE * 2, 0, 0, NULL, 0));
}

void barcode_handler_task(void *pvParameters)
{
    (void)pvParameters;

    uint8_t data[128];
    barcode_sample_t sample = {0};
    uint32_t scan_count = 0;

    barcode_uart_init();
    ESP_LOGI(TAG, "barcode task running in polling mode");

    while (1) {
        int read_len = uart_read_bytes(BARCODE_UART_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(200));
        if (read_len <= 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        data[read_len] = '\0';
        sample.timestamp_ms = get_timestamp_ms();
        strncpy(sample.barcode, (const char *)data, sizeof(sample.barcode) - 1);
        sample.barcode[sizeof(sample.barcode) - 1] = '\0';
        sample.barcode[strcspn(sample.barcode, "\r\n")] = '\0';

        if (xQueueSend(barcode_queue, &sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "barcode_queue full, sample dropped");
        }

        scan_count++;
        ESP_LOGI(TAG, "Barcode scan #%lu: %s -> STL file ID: %d",
                 (unsigned long)scan_count,
                 sample.barcode,
                 barcode_lookup_stl_file(sample.barcode));
    }
}

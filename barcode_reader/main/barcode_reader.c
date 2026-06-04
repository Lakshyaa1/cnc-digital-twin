#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
const uart_port_t uart_num = UART_NUM_2;
#define tx_d GPIO_NUM_17
#define rx_d GPIO_NUM_16
#define LOOPBACK_TEST 0
void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_d, rx_d, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024 * 2, 0, 0, NULL, 0));

    uint8_t data[128];
    TickType_t last_send = 0;
    while (1) {
#if LOOPBACK_TEST
        TickType_t now = xTaskGetTickCount();
        if (now - last_send > pdMS_TO_TICKS(1000)) {
            const char *msg = "UART2_TEST\r\n";
            uart_write_bytes(uart_num, msg, strlen(msg));
            last_send = now;
        }
#endif
        int read_len = uart_read_bytes(uart_num, data, sizeof(data), pdMS_TO_TICKS(200));
        if (read_len > 0) {
            printf("Read %d bytes: ", read_len);
            for (int i = 0; i < read_len; i++) {
                printf("%02X ", data[i]);
            }
            printf("| ");
            for (int i = 0; i < read_len; i++) {
                char c = (data[i] >= 32 && data[i] <= 126) ? (char)data[i] : '.';
                printf("%c", c);
            }
            printf("\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

}

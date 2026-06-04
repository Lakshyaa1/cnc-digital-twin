#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "esp_err.h"

// =====================
// SPI Pins
// =====================

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5

// =====================
// KX134 Registers
// =====================

#define KX134_XOUT_L  0x08
#define KX134_CNTL1   0x1B
#define KX134_ODCNTL  0x21

static spi_device_handle_t kx134;

// =====================
// Register Write
// =====================

void kx134_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = {
        reg & 0x7F,
        value
    };

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };

    ESP_ERROR_CHECK(
        spi_device_transmit(
            kx134,
            &t));
}

// =====================
// Burst Read
// =====================

void kx134_read_multi(
    uint8_t reg,
    uint8_t *data,
    size_t len)
{
    uint8_t tx[len + 1];
    uint8_t rx[len + 1];

    memset(tx, 0, sizeof(tx));

    tx[0] = reg | 0x80;

    spi_transaction_t t = {
        .length = (len + 1) * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    ESP_ERROR_CHECK(
        spi_device_transmit(
            kx134,
            &t));

    memcpy(data, &rx[1], len);
}

// =====================
// Main
// =====================

void app_main(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(
            SPI2_HOST,
            &buscfg,
            SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(
        spi_bus_add_device(
            SPI2_HOST,
            &devcfg,
            &kx134));

    printf("SPI Initialized\n");

    // ODR = 100 Hz
    kx134_write_reg(
        KX134_ODCNTL,
        0x02);

    // High Resolution
    // ±8g
    // Operating Mode
    kx134_write_reg(
        KX134_CNTL1,
        0xC0);

    vTaskDelay(
        pdMS_TO_TICKS(100));

    printf("KX134 Initialized\n");

    while (1)
    {
        uint8_t buf[6];

        kx134_read_multi(
            KX134_XOUT_L,
            buf,
            6);

        int16_t x =
            (int16_t)(
                ((uint16_t)buf[1] << 8) |
                buf[0]);

        int16_t y =
            (int16_t)(
                ((uint16_t)buf[3] << 8) |
                buf[2]);

        int16_t z =
            (int16_t)(
                ((uint16_t)buf[5] << 8) |
                buf[4]);

        float x_g =
            (float)x / 4096.0f;

        float y_g =
            (float)y / 4096.0f;

        float z_g =
            (float)z / 4096.0f;

        printf(
            "X: %.3f g | Y: %.3f g | Z: %.3f g\n",
            x_g,
            y_g,
            z_g);

        vTaskDelay(
            pdMS_TO_TICKS(100));
    }
}
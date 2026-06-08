#include "cnc_firmware_rtos.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>
#include <driver/gpio.h>
static const char *TAG = "ACCEL";

#define KX134_COUNTS_PER_G   4096.0f

#define KX134_SPI_HOST      SPI2_HOST
#define KX134_SPI_MOSI      GPIO_NUM_23
#define KX134_SPI_MISO      GPIO_NUM_19
#define KX134_SPI_CLK       GPIO_NUM_18
#define KX134_SPI_CS        GPIO_NUM_5
#define KX134_SPI_FREQ_HZ   1000000

#define KX134_XOUT_L        0x08
#define KX134_CNTL1         0x1B
#define KX134_ODCNTL        0x21

static spi_device_handle_t kx134_spi = NULL;

static void kx134_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = {reg & 0x7F, value};
    spi_transaction_t transaction = {
        .length = 16,
        .tx_buffer = tx,
    };

    ESP_ERROR_CHECK(spi_device_transmit(kx134_spi, &transaction));
}

static void kx134_read_multi(uint8_t reg, uint8_t *data, size_t len)
{
    uint8_t tx[7] = {0};
    uint8_t rx[7] = {0};
    spi_transaction_t transaction = {
        .length = (len + 1) * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    tx[0] = reg | 0x80;
    ESP_ERROR_CHECK(spi_device_transmit(kx134_spi, &transaction));
    memcpy(data, &rx[1], len);
}

static void kx134_init(void)
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = KX134_SPI_MOSI,
        .miso_io_num = KX134_SPI_MISO,
        .sclk_io_num = KX134_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t device_config = {
        .clock_speed_hz = KX134_SPI_FREQ_HZ,
        .mode = 0,
        .spics_io_num = KX134_SPI_CS,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(KX134_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(KX134_SPI_HOST, &device_config, &kx134_spi));

    kx134_write_reg(KX134_ODCNTL, 0x02);
    kx134_write_reg(KX134_CNTL1, 0xC0);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "KX134 ready on SPI2");
}

static void kx134_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t buffer[6];

    kx134_read_multi(KX134_XOUT_L, buffer, sizeof(buffer));
    *x = (int16_t)(((uint16_t)buffer[1] << 8) | buffer[0]);
    *y = (int16_t)(((uint16_t)buffer[3] << 8) | buffer[2]);
    *z = (int16_t)(((uint16_t)buffer[5] << 8) | buffer[4]);
}

void accel_task(void *pvParameters)
{
    (void)pvParameters;

    kx134_init();

    accel_sample_t sample = {0};
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10);
    uint32_t sample_count = 0;

    ESP_LOGI(TAG, "accel_task running in polling mode at 100 Hz");

    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        kx134_read_accel(&sample.x, &sample.y, &sample.z);
        sample.timestamp_ms = get_timestamp_ms();

        if (xQueueSend(accel_queue, &sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "accel_queue full, sample dropped");
        }

        sample_count++;
        if ((sample_count % 10U) == 0U) {
            ESP_LOGD(
                TAG,
                "accel: x=%.3f g y=%.3f g z=%.3f g",
                sample.x / KX134_COUNTS_PER_G,
                sample.y / KX134_COUNTS_PER_G,
                sample.z / KX134_COUNTS_PER_G);
        }
    }
}

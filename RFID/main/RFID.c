#include <stdio.h>
#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_timer.h"

#define VersionReg                0x37
#define TxControlReg              0x14
#define TxASKReg                  0x15
#define FIFOLevelReg              0x0A
#define FIFODataReg               0x09
#define BitFramingReg             0x0D
#define CommandReg                0x01
#define ComIrqReg                 0x04
#define ErrorReg                  0x06

#define RC522_CMD_IDLE            0x00
#define RC522_CMD_TRANSCEIVE      0x0C

#define RC522_REQA                0x26

#define RC522_TXCONTROL_RF_ON     0x03
#define RC522_TXASK_FORCE100ASK   0x40
#define RC522_FIFO_FLUSH          0x80

#define RC522_BITFRAMING_STARTSEND       0x80
#define RC522_BITFRAMING_TXLASTBITS_MASK 0x07
#define RC522_BITFRAMING_TXLASTBITS_7    0x07

#define RC522_COMIRQ_WAIT_MASK    0x30
#define RC522_COMIRQ_CLEAR_ALL    0x7F

#define RC522_REQA_TIMEOUT_US     50000

typedef struct
{
    spi_device_handle_t spi;
} rc522_t;

uint8_t rc522_read_reg(rc522_t *dev, uint8_t reg)
{
    uint8_t tx[2] = {
        ((reg << 1) & 0x7E) | 0x80,
        0x00
    };

    uint8_t rx[2] = {0};

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    ESP_ERROR_CHECK(spi_device_transmit(dev->spi, &t));

    return rx[1];
}

void rc522_write_reg(rc522_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = {
        ((reg << 1) & 0x7E),
        value
    };

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };

    ESP_ERROR_CHECK(spi_device_transmit(dev->spi, &t));
}

esp_err_t rc522_init(rc522_t *dev)
{
    if (dev == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * The RC522 is already out of reset because RST is tied to 3.3 V.
     * Read VersionReg to confirm SPI is working and the chip is alive.
     */
    uint8_t version = rc522_read_reg(dev, VersionReg);
    printf("VersionReg = 0x%02X\n", version);

    if (version != 0x91 && version != 0x92)
    {
        return ESP_ERR_NOT_FOUND;
    }

    /*
     * Put the RC522 in Idle so we start from a known state before setup.
     */
    rc522_write_reg(dev, CommandReg, RC522_CMD_IDLE);

    /*
     * Enable the RF field drivers.
     * TxControlReg bit0 = Tx1RFEn, bit1 = Tx2RFEn.
     * Both bits must be set to drive the antenna and generate the field.
     */
    uint8_t value = rc522_read_reg(dev, TxControlReg);
    printf("TxControlReg Before = 0x%02X\n", value);
    value |= RC522_TXCONTROL_RF_ON;
    rc522_write_reg(dev, TxControlReg, value);
    value = rc522_read_reg(dev, TxControlReg);
    printf("TxControlReg After = 0x%02X\n", value);

    /*
     * Force 100% ASK modulation required by ISO14443A.
     * TxASKReg bit6 = Force100ASK.
     */
    value = rc522_read_reg(dev, TxASKReg);
    printf("TxASKReg Before = 0x%02X\n", value);
    value |= RC522_TXASK_FORCE100ASK;
    rc522_write_reg(dev, TxASKReg, value);
    value = rc522_read_reg(dev, TxASKReg);
    printf("TxASKReg After = 0x%02X\n", value);

    /*
     * Clear any leftover bytes from previous activity.
     * FIFOLevelReg bit7 flushes the FIFO.
     */
    rc522_write_reg(dev, FIFOLevelReg, RC522_FIFO_FLUSH);

    return ESP_OK;
}

esp_err_t rc522_request(rc522_t *dev, uint8_t *rx, size_t rx_size, size_t *rx_len)
{
    if (dev == NULL || rx == NULL || rx_len == NULL || rx_size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *rx_len = 0;

    /*
     * 1) Stop any running command to avoid mixing states.
     */
    rc522_write_reg(dev, CommandReg, RC522_CMD_IDLE);

    /*
     * 2) Clear all IRQ flags so we do not see stale completion bits.
     * ComIrqReg is cleared by writing 1s to the flag positions.
     */
    rc522_write_reg(dev, ComIrqReg, RC522_COMIRQ_CLEAR_ALL);

    /*
     * 3) Flush FIFO before loading REQA to ensure a clean TX buffer.
     */
    rc522_write_reg(dev, FIFOLevelReg, RC522_FIFO_FLUSH);

    /*
     * 4) Load REQA (0x26) into FIFO. This is a 7-bit command.
     */
    rc522_write_reg(dev, FIFODataReg, RC522_REQA);

    /*
     * 5) Configure BitFramingReg for a 7-bit transmission.
     * BitFramingReg bits 2:0 = TxLastBits, and bit7 must be clear.
     */
    uint8_t value = rc522_read_reg(dev, BitFramingReg);
    value &= (uint8_t)~RC522_BITFRAMING_STARTSEND;
    value = (value & ~RC522_BITFRAMING_TXLASTBITS_MASK) | RC522_BITFRAMING_TXLASTBITS_7;
    rc522_write_reg(dev, BitFramingReg, value);

    /*
     * 6) Start the TRANSCEIVE command.
     */
    rc522_write_reg(dev, CommandReg, RC522_CMD_TRANSCEIVE);

    /*
     * 7) Set StartSend (BitFramingReg bit7) to trigger the transmission.
     */
    value = rc522_read_reg(dev, BitFramingReg);
    value |= RC522_BITFRAMING_STARTSEND;
    rc522_write_reg(dev, BitFramingReg, value);

    /*
     * 8) Poll ComIrqReg until RxIRq or IdleIRq is set, or until timeout.
     * This gives the RC522 time to transmit REQA and receive ATQA.
     */
    int64_t start_us = esp_timer_get_time();
    uint8_t irq = 0;

    while (true)
    {
        irq = rc522_read_reg(dev, ComIrqReg);
        if (irq & RC522_COMIRQ_WAIT_MASK)
        {
            break;
        }

        if ((esp_timer_get_time() - start_us) > RC522_REQA_TIMEOUT_US)
        {
            rc522_write_reg(dev, CommandReg, RC522_CMD_IDLE);
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /*
     * 9) Clear StartSend so later commands are not affected.
     */
    value = rc522_read_reg(dev, BitFramingReg);
    value &= (uint8_t)~RC522_BITFRAMING_STARTSEND;
    rc522_write_reg(dev, BitFramingReg, value);

    /*
     * 10) Check ErrorReg for protocol, parity, or CRC errors.
     * Any non-zero value indicates the exchange failed.
     */
    uint8_t err = rc522_read_reg(dev, ErrorReg);
    if (err != 0)
    {
        return ESP_FAIL;
    }

    /*
     * 11) Read FIFOLevelReg to see how many bytes the RC522 received.
     */
    uint8_t count = rc522_read_reg(dev, FIFOLevelReg);
    if (count == 0)
    {
        return ESP_ERR_NOT_FOUND;
    }

    /*
     * 12) Read the response bytes from FIFODataReg.
     * ATQA is expected to be 2 bytes, but we respect rx_size and still
     * drain the FIFO to keep the internal state clean.
     */
    size_t stored = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t data = rc522_read_reg(dev, FIFODataReg);
        if (stored < rx_size)
        {
            rx[stored++] = data;
        }
    }

    *rx_len = stored;

    /*
     * 13) Return to Idle to leave the RC522 in a known state.
     */
    rc522_write_reg(dev, CommandReg, RC522_CMD_IDLE);

    return ESP_OK;
}

void app_main(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_NUM_23,
        .miso_io_num = GPIO_NUM_19,
        .sclk_io_num = GPIO_NUM_18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(
        SPI2_HOST,
        &buscfg,
        SPI_DMA_CH_AUTO
    ));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = GPIO_NUM_5,
        .queue_size = 1,
    };

    spi_device_handle_t rc522_spi = NULL;
    ESP_ERROR_CHECK(spi_bus_add_device(
        SPI2_HOST,
        &devcfg,
        &rc522_spi
    ));

    rc522_t rc522 = {
        .spi = rc522_spi,
    };

    esp_err_t err = rc522_init(&rc522);
    if (err != ESP_OK)
    {
        printf("RC522 init failed: %s\n", esp_err_to_name(err));
        return;
    }

    printf("RC522 detected and initialized\n");

    while (1)
    {
        uint8_t atqa[2] = {0};
        size_t atqa_len = 0;

        err = rc522_request(&rc522, atqa, sizeof(atqa), &atqa_len);

        if (err == ESP_OK)
        {
            printf("ATQA (%u bytes):", (unsigned)atqa_len);
            for (size_t i = 0; i < atqa_len; i++)
            {
                printf(" 0x%02X", atqa[i]);
            }
            printf("\n");
        }
        else if (err == ESP_ERR_NOT_FOUND)
        {
            printf("No card in field\n");
        }
        else if (err == ESP_ERR_TIMEOUT)
        {
            printf("REQA timeout\n");
        }
        else
        {
            printf("REQA failed: %s\n", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
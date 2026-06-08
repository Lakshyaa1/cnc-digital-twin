#include "cnc_firmware_rtos.h"
#include "driver/rc522_spi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "rc522.h"
#include "rc522_picc.h"
#include <string.h>

static const char *TAG = "RFID";
#define RC522_SPI_BUS_GPIO_MOSI   25
#define RC522_SPI_BUS_GPIO_MISO   26
#define RC522_SPI_BUS_GPIO_SCLK   27
#define RC522_SPI_SCANNER_GPIO_SDA 14
#define RC522_SCANNER_GPIO_RST   (-1)

static rc522_spi_config_t driver_config = {
    .host_id = SPI3_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
        .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
        .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
    },
    .dev_config = {
        .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
    },
    .rst_io_num = RC522_SCANNER_GPIO_RST,
};

static rc522_driver_handle_t driver = NULL;
static rc522_handle_t scanner = NULL;

static uint8_t uid_is_authorized(const char *uid_str)
{
    return strstr(uid_str, "04 12 AB CD") != NULL ? 1U : 0U;
}

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;
    char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX] = {0};
    rfid_sample_t sample = {0};
    uint8_t authenticated;

    (void)arg;
    (void)base;
    (void)event_id;

    if (picc->state != RC522_PICC_STATE_ACTIVE && picc->state != RC522_PICC_STATE_ACTIVE_H) {
        if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
            ESP_LOGI(TAG, "Card removed");
        }
        return;
    }

    if (rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str)) != ESP_OK) {
        ESP_LOGW(TAG, "UID conversion failed");
        return;
    }

    strncpy(sample.rfid_uid, uid_str, sizeof(sample.rfid_uid) - 1);
    sample.uid_length = (uint8_t)strlen(sample.rfid_uid);
    sample.timestamp_ms = get_timestamp_ms();
    authenticated = uid_is_authorized(sample.rfid_uid);

    sensor_data_lock();
    shared_sensor_data.last_rfid = sample;
    shared_sensor_data.user_authenticated = authenticated;
    shared_sensor_data.last_update_ms = sample.timestamp_ms;
    sensor_data_unlock();

    if (xQueueSend(rfid_queue, &sample, 0) != pdPASS) {
        ESP_LOGW(TAG, "rfid_queue full, sample dropped");
    }

    ESP_LOGI(TAG, "Card detected, UID: %s Auth=%s", sample.rfid_uid, authenticated ? "PASS" : "FAIL");
}

void rfid_handler_task(void *pvParameters)
{
    rc522_config_t scanner_config = {
        .driver = NULL,
        .poll_interval_ms = 120,
        .task_stack_size = 4 * 1024,
        .task_priority = 3,
        .task_mutex = NULL,
    };

    (void)pvParameters;

    ESP_ERROR_CHECK(rc522_spi_create(&driver_config, &driver));
    ESP_ERROR_CHECK(rc522_driver_install(driver));

    scanner_config.driver = driver;
    ESP_ERROR_CHECK(rc522_create(&scanner_config, &scanner));
    ESP_ERROR_CHECK(rc522_register_events(
        scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL));
    ESP_ERROR_CHECK(rc522_start(scanner));

    ESP_LOGI(TAG, "RFID_Open scanner started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGD(TAG, "Scanning for cards...");
    }
}

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "rc522.h"
#include "rc522_picc.h"
#include "driver/rc522_spi.h"

static const char *TAG = "rc522-app";

// SPI pin mapping for ESP32 (VSPI)
#define RC522_SPI_BUS_GPIO_MOSI (23)
#define RC522_SPI_BUS_GPIO_MISO (19)
#define RC522_SPI_BUS_GPIO_SCLK (18)
#define RC522_SPI_SCANNER_GPIO_SDA (5)
#define RC522_SCANNER_GPIO_RST (-1) // RST is tied to 3.3V

// SPI driver configuration for the RC522 component
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

static rc522_driver_handle_t driver;
static rc522_handle_t scanner;

static void scanning_heartbeat_task(void *arg)
{
	(void)arg;

	while (true) {
		ESP_LOGI(TAG, "Scanning for cards...");
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
}

// Event callback invoked when PICC (card) state changes
static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
	(void)arg;
	(void)base;
	(void)event_id;

	rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
	rc522_picc_t *picc = event->picc;

	if (picc->state == RC522_PICC_STATE_ACTIVE || picc->state == RC522_PICC_STATE_ACTIVE_H) {
		// Card is selected and active, print UID
		char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];
		if (rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str)) == ESP_OK) {
			ESP_LOGI(TAG, "Card detected, UID: %s", uid_str);
		}
		else {
			ESP_LOGW(TAG, "Card detected, UID conversion failed");
		}
	}
	else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
		// Card left the field
		ESP_LOGI(TAG, "Card removed");
	}
}

void app_main(void)
{
	// Create and install the RC522 SPI driver
	ESP_ERROR_CHECK(rc522_spi_create(&driver_config, &driver));
	ESP_ERROR_CHECK(rc522_driver_install(driver));

	// Create the RC522 scanner instance
	rc522_config_t scanner_config = {
		.driver = driver,
		.poll_interval_ms = 120,
		.task_stack_size = 4 * 1024,
		.task_priority = 3,
		.task_mutex = NULL,
	};

	ESP_ERROR_CHECK(rc522_create(&scanner_config, &scanner));

	// Register PICC state change events
	ESP_ERROR_CHECK(rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL));

	// Start the polling task
	ESP_ERROR_CHECK(rc522_start(scanner));

	ESP_LOGI(TAG, "RC522 scanner started");

	xTaskCreate(scanning_heartbeat_task, "rc522_heartbeat", 2048, NULL, 1, NULL);
}

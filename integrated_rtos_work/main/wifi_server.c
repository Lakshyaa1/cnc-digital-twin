#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_server.h"

#include "wifi_server.h"
#include "cnc_firmware_rtos.h"
static const char *TAG = "WIFI_SERVER";

#define WIFI_SSID      "NCAIR IOT"
#define WIFI_PASSWORD  "Asim@123Tewari"

static httpd_handle_t server = NULL;
bool wifi_connected = false;
static esp_err_t telemetry_get_handler(httpd_req_t *req)
{
    char response[1024];

    sensor_data_lock();

    snprintf(
        response,
        sizeof(response),
        "{"
        "\"temperature_c\": %.2f,"
        "\"pressure_pa\": %.2f,"
        "\"analog_pressure_bar\": %.2f,"
        "\"distance_cm\": %.2f,"
        "\"water_detected\": %s,"
        "\"authenticated\": %s,"
        "\"accel_x\": %d,"
        "\"accel_y\": %d,"
        "\"accel_z\": %d,"
        "\"last_update_ms\": %llu"
        "}",
        shared_sensor_data.last_bmp280.temperature_c,
        shared_sensor_data.last_bmp280.pressure_pa,
        shared_sensor_data.last_analog_pressure.analog_pressure_bar,
        shared_sensor_data.last_ultrasonic.distance_cm,
        shared_sensor_data.last_water_level.water_detected ? "true" : "false",
        shared_sensor_data.user_authenticated ? "true" : "false",
        shared_sensor_data.last_accel.x,
        shared_sensor_data.last_accel.y,
        shared_sensor_data.last_accel.z,
        shared_sensor_data.last_update_ms
    );

    sensor_data_unlock();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t telemetry_get_handler(httpd_req_t *req);

static httpd_uri_t root = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_get_handler,
    .user_ctx = NULL
};

static httpd_uri_t telemetry_uri = {
    .uri      = "/telemetry",
    .method   = HTTP_GET,
    .handler  = telemetry_get_handler,
    .user_ctx = NULL
};

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *response = "Hello from CNC Firmware";

    httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


static httpd_handle_t start_webserver(void)
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server_handle = NULL;

    if (httpd_start(&server_handle, &config) == ESP_OK)
    {
        httpd_register_uri_handler(
            server_handle,
            &root);

        httpd_register_uri_handler(
            server_handle,
            &telemetry_uri);

        ESP_LOGI(TAG,
                 "HTTP server started");

        return server_handle;
    }

    ESP_LOGE(TAG,
             "HTTP server failed to start");

    return NULL;
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG,
                 "WiFi started");

        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG,
                 "Connected to router");
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;

        ESP_LOGW(TAG,
                 "Disconnected, reconnecting...");

        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connected = true;

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG,
                 "Got IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        if (server == NULL)
        {
            server =
                start_webserver();
        }
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(
        esp_netif_init());

    ESP_ERROR_CHECK(
        esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config));

    ESP_ERROR_CHECK(
        esp_wifi_start());

    ESP_LOGI(TAG,
             "wifi_init_sta finished");
}

void wifi_server_start(void)
{
    wifi_init_sta();
}

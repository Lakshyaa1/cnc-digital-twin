#include "mqtt_task.h"
#include "cnc_firmware_rtos.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";

#define MQTT_BROKER_URI "mqtt://192.168.1.100"

static esp_mqtt_client_handle_t mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected");
            break;

        default:
            break;
    }
}

void mqtt_task(void *pvParameters)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL);

    esp_mqtt_client_start(mqtt_client);

    char payload[512];

    while (1)
    {
        sensor_data_lock();

        snprintf(
            payload,
            sizeof(payload),
            "{"
            "\"temperature\":%.2f,"
            "\"pressure\":%.2f,"
            "\"analog_pressure\":%.2f,"
            "\"distance\":%.2f,"
            "\"water\":%d,"
            "\"authenticated\":%d,"
            "\"accel_x\":%d,"
            "\"accel_y\":%d,"
            "\"accel_z\":%d"
            "}",
            shared_sensor_data.last_bmp280.temperature_c,
            shared_sensor_data.last_bmp280.pressure_pa,
            shared_sensor_data.last_analog_pressure.analog_pressure_bar,
            shared_sensor_data.last_ultrasonic.distance_cm,
            shared_sensor_data.last_water_level.water_detected,
            shared_sensor_data.user_authenticated,
            shared_sensor_data.last_accel.x,
            shared_sensor_data.last_accel.y,
            shared_sensor_data.last_accel.z);

        sensor_data_unlock();

        esp_mqtt_client_publish(
            mqtt_client,
            "cnc/telemetry",
            payload,
            0,
            1,
            0);

        ESP_LOGI(TAG, "Published: %s", payload);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

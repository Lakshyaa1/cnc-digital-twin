#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void sensor_task(void *pvParameters)
{
    QueueHandle_t queue =
        (QueueHandle_t)pvParameters;

    int value = 0;

    while (1)
    {
        value++;

        xQueueSend(
            queue,
            &value,
            portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void logger_task(void *pvParameters)
{
    QueueHandle_t queue =
        (QueueHandle_t)pvParameters;

    int received;

    while (1)
    {
        xQueueReceive(
            queue,
            &received,
            portMAX_DELAY);

        printf("Received: %d\n", received);
    }
}
void app_main(void)
{
    QueueHandle_t sensor_queue;

    sensor_queue = xQueueCreate(
        5,
        sizeof(int));

    xTaskCreate(
        sensor_task,
        "Sensor",
        2048,
        sensor_queue,
        1,
        NULL);

    xTaskCreate(
        logger_task,
        "Logger",
        2048,
        sensor_queue,
        1,
        NULL);
}
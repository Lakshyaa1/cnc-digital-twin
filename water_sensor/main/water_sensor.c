#include <stdio.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
void app_main(void)
{
gpio_config_t io_conf=
{
     .pin_bit_mask = (1ULL << GPIO_NUM_4),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    gpio_config(&io_conf)};
    while(1)
    {
    int level=gpio_get_level(GPIO_NUM_4);
    if(level==1)
    {printf("Water detected\n");
    }
    else
    {
        printf("No water detected\n");
}
vTaskDelay(pdMS_TO_TICKS(1000));
    }

}



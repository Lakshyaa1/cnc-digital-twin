#include <stdio.h>
#include "driver/i2c_master.h"
#include <stdint.h>
void app_main(void)
{
i2c_master_bus_config_t bus_config = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus_handle;
i2c_new_master_bus(&bus_config, 
    &bus_handle);
    i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x76,
    .scl_speed_hz = 100000,
};
i2c_master_dev_handle_t bmp_handle;
i2c_master_bus_add_device(
    bus_handle,
    &dev_config,
    &bmp_handle
);
uint8_t buffer;
uint8_t reg_addr = 0xD0;
i2c_master_transmit_receive(bmp_handle, &reg_addr,1, &buffer,1,-1);
if(buffer==0x58)
{ 
    printf("BMP280 sensor found!\n");
    uint8_t config_data[2] = {0xF4, 0x27};
    i2c_master_transmit(bmp_handle, config_data,2,-1);
    uint8_t data[3];
    uint8_t temp_reg_addr = 0xFA;
    i2c_master_transmit_receive(bmp_handle, &temp_reg_addr,1, data,3,-1);
    int32_t raw_temp =
    (data[0] << 12) |
    (data[1] << 4)  |
    (data[2] >> 4);
    printf("Raw temperature data: %ld\n", raw_temp);
    
    uint8_t calib_reg_addr = 0x88;

uint8_t calib_data[6];

i2c_master_transmit_receive(
    bmp_handle,
    &calib_reg_addr,
    1,
    calib_data,
    6,
    -1
);

uint16_t dig_T1 =
    calib_data[0] |
    (calib_data[1] << 8);

int16_t dig_T2 =
    calib_data[2] |
    (calib_data[3] << 8);

int16_t dig_T3 =
    calib_data[4] |
    (calib_data[5] << 8);

printf("dig_T1 = %u\n", dig_T1);
printf("dig_T2 = %d\n", dig_T2);
printf("dig_T3 = %d\n", dig_T3);
   double var1, var2, t_fine, T;

var1 =
(((double)raw_temp) / 16384.0 -
(((double)dig_T1) / 1024.0)) *
((double)dig_T2);

var2 =
((((double)raw_temp) / 131072.0 -
(((double)dig_T1) / 8192.0)) *
((((double)raw_temp) / 131072.0 -
(((double)dig_T1) / 8192.0)))) *
((double)dig_T3);

t_fine = var1 + var2;

T = (var1 + var2) / 5120.0;

printf("Temperature = %.2f C\n", T);
 uint8_t pdata[3];
uint8_t press_reg_addr = 0xF7;
    i2c_master_transmit_receive(bmp_handle, &press_reg_addr,1, pdata,3,-1);
    int32_t raw_press =
    (pdata[0] << 12) |
    (pdata[1] << 4)  |
    (pdata[2] >> 4);
    printf("Raw pressure data: %ld\n", raw_press);
    uint8_t pcalib_reg_addr = 0x8E;
uint8_t pcalib_data[18];
i2c_master_transmit_receive(
    bmp_handle,
    &pcalib_reg_addr,
    1,
    pcalib_data,
    18,
    -1
);
uint16_t dig_P1 =
    pcalib_data[0] |
    (pcalib_data[1] << 8); 
int16_t dig_P2 =
    pcalib_data[2] |
    (pcalib_data[3] << 8);
int16_t dig_P3 =        
    pcalib_data[4] |
    (pcalib_data[5] << 8);
int16_t dig_P4 =
    pcalib_data[6] |
    (pcalib_data[7] << 8);
int16_t dig_P5 =
    pcalib_data[8] |
    (pcalib_data[9] << 8);
int16_t dig_P6 =
    pcalib_data[10] |
    (pcalib_data[11] << 8);
int16_t dig_P7 =
    pcalib_data[12] |
    (pcalib_data[13] << 8);
int16_t dig_P8 =
    pcalib_data[14] |
    (pcalib_data[15] << 8);
int16_t dig_P9 =
    pcalib_data[16] |
    (pcalib_data[17] << 8);
printf("dig_P1 = %u\n", dig_P1);
printf("dig_P2 = %d\n", dig_P2);
printf("dig_P3 = %d\n", dig_P3);
printf("dig_P4 = %d\n", dig_P4);
printf("dig_P5 = %d\n", dig_P5);
printf("dig_P6 = %d\n", dig_P6);
printf("dig_P7 = %d\n", dig_P7);
printf("dig_P8 = %d\n", dig_P8);
printf("dig_P9 = %d\n", dig_P9);
double pvar1, pvar2, pressure;

pvar1 = (t_fine / 2.0) - 64000.0;

pvar2 =
pvar1 * pvar1 *
((double)dig_P6) / 32768.0;

pvar2 =
pvar2 +
pvar1 * ((double)dig_P5) * 2.0;

pvar2 =
(pvar2 / 4.0) +
(((double)dig_P4) * 65536.0);

pvar1 =
(
    (
        ((double)dig_P3) *
        pvar1 * pvar1 / 524288.0
    ) +
    (
        ((double)dig_P2) *
        pvar1
    )
) / 524288.0;

pvar1 =
(1.0 + pvar1 / 32768.0) *
((double)dig_P1);

pressure =
1048576.0 - (double)raw_press;

pressure =
(pressure - (pvar2 / 4096.0)) *
6250.0 / pvar1;

pvar1 =
((double)dig_P9) *
pressure * pressure /
2147483648.0;

pvar2 =
pressure *
((double)dig_P8) /
32768.0;

pressure =
pressure +
(pvar1 + pvar2 +
((double)dig_P7)) / 16.0;

printf(
    "Pressure = %.2f Pa\n",
    pressure
);


}
else
{
    printf("BMP280 sensor not found!\n");

}
}
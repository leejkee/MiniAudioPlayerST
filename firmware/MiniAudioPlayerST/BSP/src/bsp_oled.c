#include "bsp_oled.h"
#include "i2c.h"

#define OLED_I2C_ADDR (0x3C << 1)
#define OLED_TIMEOUT 100


void BSP_OLED_WriteCmd(uint8_t command)
{
    // 通过 I2C 发送命令到 OLED
    uint8_t data[2] = {0x00, command}; // 0x00 表示命令模式
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, data, 2, OLED_TIMEOUT);
}

void BSP_OLED_WriteData(uint8_t *data, uint16_t len)
{
    // 通过 I2C 发送数据到 OLED
    uint8_t ctrl = 0x40; // 0x40 表示数据模式
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, &ctrl, 1, OLED_TIMEOUT);
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, data, len, HAL_MAX_DELAY);
}


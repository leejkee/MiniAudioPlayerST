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
    // 控制字节 + 数据必须在同一次 I2C 事务中发送
    // SSD1315 要求每笔 I2C 写都以控制字节开头
    static uint8_t buf[129];  // 1 控制字节 + 最大 128 数据字节
    buf[0] = 0x40;            // 0x40 表示数据模式 (Co=0, D/C#=1)
    for (uint16_t i = 0; i < len && i < 128; i++) {
        buf[i + 1] = data[i];
    }
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, buf, len + 1, HAL_MAX_DELAY);
}


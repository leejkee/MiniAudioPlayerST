#include "bsp_i2c_scanner.h"
#include "bsp_config.h"


void BSP_I2C_Scanner_Test(I2C_HandleTypeDef *hi2c, uint32_t timeout)
{
    uint8_t test_count = 5;
    while (test_count--) {
        BSP_DEBUG_PRINTF("[I2C] Test %u: Scanning bus ...\r\n", 5 - test_count);
        uint8_t found = BSP_I2C_Scanner_Scan(hi2c, timeout);
        BSP_DEBUG_PRINTF("[I2C] Test %u: Found %u device(s)\r\n", 5 - test_count, found);
        HAL_Delay(1000); // 延时 1 秒
    }
}

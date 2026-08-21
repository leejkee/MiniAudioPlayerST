#include "iic_scan_test.h"
#include "bsp_i2c_scanner.h"
#include "bsp_config.h"
#define LOG_MODULE "I2CScanTest"


void I2C_Scanner_Test(I2C_HandleTypeDef *hi2c, uint32_t timeout)
{
    uint8_t test_count = 5;
    while (test_count--) {
        LOG_DEBUG("I2C", "Test %u: Scanning bus ...", 5 - test_count);
        uint8_t found = BSP_I2C_Scanner_Scan(hi2c, timeout);
        LOG_DEBUG("I2C", "Test %u: Found %u device(s)", 5 - test_count, found);
        HAL_Delay(1000); // 延时 1 秒
    }
}

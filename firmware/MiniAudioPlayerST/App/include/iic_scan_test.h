#ifndef __IIC_SCAN_TEST_H__
#define __IIC_SCAN_TEST_H__
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"

void BSP_I2C_Scanner_Test(I2C_HandleTypeDef *hi2c, uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* __IIC_SCAN_TEST_H__ */

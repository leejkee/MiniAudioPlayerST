#ifndef __BSP_OLED_H__
#define __BSP_OLED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"


void BSP_OLED_WriteCmd(uint8_t command);

void BSP_OLED_WriteData(uint8_t *data, uint16_t len);


#ifdef __cplusplus
}
#endif

#endif

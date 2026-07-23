#ifndef __SD_CARD_H__
#define __SD_CARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void SD_Mount(void);
void SD_Unmount(void);
void SD_ReadFile(const char *filename, uint8_t *buffer, uint32_t len);


#ifdef __cplusplus
}
#endif

#endif
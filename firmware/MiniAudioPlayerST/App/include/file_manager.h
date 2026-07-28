#ifndef __FILE_MANAGER_H
#define __FILE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "sd_card.h"

void FileManager_Init(void);
void FileManager_MoveCursorUp(void);
void FileManager_MoveCursorDown(void);
uint8_t FileManager_GetVisibleCount(void);
uint8_t FileManager_GetCursorRow(void);
const WCHAR *FileManager_GetVisibleEntry(uint8_t row);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_MANAGER_H */

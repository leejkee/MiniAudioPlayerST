#ifndef __FILE_MANAGER_H
#define __FILE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "sd_card.h"

SD_Status_t FileManager_Init(void);
void FileManager_MoveCursorUp(void);
void FileManager_MoveCursorDown(void);
uint8_t FileManager_GetVisibleCount(void);
uint8_t FileManager_GetCursorRow(void);
const WCHAR *FileManager_GetVisibleEntry(uint8_t row);
uint32_t FileManager_GetFileCount(void);
uint32_t FileManager_GetSelectedIndex(void);
uint8_t FileManager_SelectIndex(uint32_t index);
uint8_t FileManager_GetPath(uint32_t index,
                            WCHAR *path,
                            uint16_t path_capacity);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_MANAGER_H */

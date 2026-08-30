#ifndef __FILE_MANAGER_H
#define __FILE_MANAGER_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "sd_card.h"

SD_Status_t  FileManager_Init(void);
void         FileManager_MoveCursorUp(void);
void         FileManager_MoveCursorDown(void);
uint8_t      FileManager_GetVisibleCount(void);
uint8_t      FileManager_GetCursorRow(void);
const WCHAR *FileManager_GetVisibleEntry(uint8_t row);
uint32_t     FileManager_GetFileCount(void);
uint32_t     FileManager_GetSelectedIndex(void);
uint8_t      FileManager_SelectIndex(uint32_t index);
uint8_t      FileManager_GetPath(uint32_t index, WCHAR *path, uint16_t path_capacity);

/**
  * @brief  打开指定索引的音频文件并将其设为当前文件
  * @note   返回的 FIL 指针由 FileManager 持有，在 FileManager_CloseCurrent() 前有效。
  */
SD_Status_t  FileManager_Open(uint32_t index, FIL **file);
void         FileManager_CloseCurrent(void);
void         FileManager_ClearCurrent(void);
FIL         *FileManager_GetCurrentFile(void);
uint32_t     FileManager_GetCurrentIndex(void);
uint8_t      FileManager_GetAdjacentIndex(int8_t direction, uint32_t *index);
uint32_t     FileManager_GetCurrentFileSize(void);
const WCHAR *FileManager_GetCurrentPath(void);

#ifdef __cplusplus
}
#endif

#endif /* __FILE_MANAGER_H */

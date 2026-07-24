#ifndef __SD_CARD_H__
#define __SD_CARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  SD 卡操作状态码
 */
typedef enum
{
    SD_OK = 0,
    SD_ERR_MOUNT,
    SD_ERR_NO_FILESYSTEM,
    SD_ERR_OPEN,
    SD_ERR_READ,
    SD_ERR_PARAM
} SD_Status_t;

/**
 * @brief  挂载 SD 卡文件系统 (FATFS f_mount)
 * @retval SD_OK 成功, 其他为错误码
 */
SD_Status_t SD_Mount(void);

/**
 * @brief  卸载 SD 卡文件系统
 */
void SD_Unmount(void);

/**
 * @brief  读取文件内容到缓冲区
 * @param  filename 文件路径 (如 "0:/README.TXT")
 * @param  buffer   输出缓冲区
 * @param  len      入: 缓冲区容量 / 出: 实际读取字节数
 * @retval SD_OK 成功
 */
SD_Status_t SD_ReadFile(const char *filename, uint8_t *buffer, uint32_t *len);

/**
 * @brief  列出目录内容, 通过 BSP_DEBUG_PRINTF 输出到串口
 * @param  path 目录路径 (如 "/" 表示根目录)
 * @retval SD_OK 成功
 * @note   依赖 BSP_DEBUG_UART=1
 */
SD_Status_t SD_ListDir(const char *path);

#ifdef __cplusplus
}
#endif

#endif

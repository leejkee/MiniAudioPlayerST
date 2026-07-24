/**
  ******************************************************************************
  * @file    SD_Card.c
  * @brief   SD 卡应用层 — 基于 FatFs 的文件系统操作封装
  * @note    依赖 FATFS 层 (fatfs.h), 不直接调用 BSP_SD。
  *          当 _LFN_UNICODE=1 时内部将 char* 路径转为 TCHAR (WCHAR),
  *          LFN 为 UTF-16LE, 串口输出转 UTF-8。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "SD_Card.h"
#include "fatfs.h"
#include "bsp_config.h"
#include <string.h>

/* 内部辅助 ----------------------------------------------------------------*/

/**
  * @brief  将 FatFs FRESULT 映射为应用层 SD_Status_t
  */
static SD_Status_t SD_MapFRESULT(FRESULT fr)
{
    switch (fr) {
        case FR_OK:            return SD_OK;
        case FR_DISK_ERR:
        case FR_NOT_READY:     return SD_ERR_MOUNT;
        case FR_NO_FILE:
        case FR_NO_PATH:       return SD_ERR_OPEN;
        case FR_NO_FILESYSTEM: return SD_ERR_NO_FILESYSTEM;
        default:               return SD_ERR_READ;
    }
}

#if _LFN_UNICODE
/*
 * ASCII char* 路径 → TCHAR (WCHAR) 路径转换
 * 驱动路径和文件名中的 ASCII 部分直接逐字符复制
 */
static void path_to_tchar(const char *src, TCHAR *dst, uint16_t max_len)
{
    uint16_t i;
    for (i = 0; i < max_len - 1 && src[i]; i++) {
        dst[i] = (WCHAR)(unsigned char)src[i];
    }
    dst[i] = 0;
}

/*
 * UTF-16LE → UTF-8 转换 (BMP only, 覆盖所有 CJK)
 */
static void utf16_to_utf8(const WCHAR *src, char *dst, uint16_t dst_size)
{
    if (!src || !dst || dst_size == 0) return;

    while (*src && dst_size > 4) {
        uint16_t ch = *src++;
        if (ch < 0x80) {
            *dst++ = (char)ch;
            dst_size--;
        } else if (ch < 0x800) {
            *dst++ = (char)(0xC0 | (ch >> 6));
            *dst++ = (char)(0x80 | (ch & 0x3F));
            dst_size -= 2;
        } else {
            *dst++ = (char)(0xE0 | (ch >> 12));
            *dst++ = (char)(0x80 | ((ch >> 6) & 0x3F));
            *dst++ = (char)(0x80 | (ch & 0x3F));
            dst_size -= 3;
        }
    }
    *dst = '\0';
}
#endif /* _LFN_UNICODE */

/* 公开函数 ----------------------------------------------------------------*/

/**
  * @brief  挂载 SD 卡文件系统
  */
SD_Status_t SD_Mount(void)
{
    FRESULT fr;

    BSP_DEBUG_PRINTF("[SD] Mounting file system...\r\n");

    fr = f_mount(&USERFatFS, TUSERPath, 1);
    if (fr != FR_OK) {
        BSP_DEBUG_PRINTF("[SD] f_mount failed: %d\r\n", fr);
        return SD_MapFRESULT(fr);
    }

    BSP_DEBUG_PRINTF("[SD] Mount OK\r\n");
    return SD_OK;
}

/**
  * @brief  卸载文件系统
  */
void SD_Unmount(void)
{
    f_mount(NULL, TUSERPath, 0);
    BSP_DEBUG_PRINTF("[SD] Unmounted\r\n");
}

/**
  * @brief  读取文件内容
  */
SD_Status_t SD_ReadFile(const char *filename, uint8_t *buffer, uint32_t *len)
{
    FRESULT fr;
    FIL     file;
    UINT    bytes_read;
#if _LFN_UNICODE
    TCHAR   tpath[_MAX_LFN + 1];
#else
    const TCHAR *tpath;
#endif

    if (filename == NULL || buffer == NULL || len == NULL) {
        return SD_ERR_PARAM;
    }

#if _LFN_UNICODE
    path_to_tchar(filename, tpath, sizeof(tpath) / sizeof(tpath[0]));
#else
    tpath = filename;
#endif

    fr = f_open(&file, tpath, FA_READ | FA_OPEN_EXISTING);
    if (fr != FR_OK) {
        BSP_DEBUG_PRINTF("[SD] f_open(%s) failed: %d\r\n", filename, fr);
        return SD_MapFRESULT(fr);
    }

    fr = f_read(&file, buffer, *len, &bytes_read);
    *len = bytes_read;

    f_close(&file);

    if (fr != FR_OK) {
        BSP_DEBUG_PRINTF("[SD] f_read failed: %d\r\n", fr);
        return SD_MapFRESULT(fr);
    }

    return SD_OK;
}

/**
  * @brief  列出目录内容, 通过 BSP_DEBUG_PRINTF 输出到串口
  */
SD_Status_t SD_ListDir(const char *path)
{
    FRESULT fr;
    DIR     dir;
    FILINFO fno;
#if _LFN_UNICODE
    TCHAR   tpath[_MAX_LFN + 1];          /* 栈: 512B */
    static WCHAR lfn_buf[_MAX_LFN + 1];   /* 静态: 512B, 不占栈 */
    static char  utf8_buf[_MAX_LFN + 1];  /* 静态: 256B, 不占栈 */
#else
    const TCHAR *tpath;
    static char  lfn_buf[_MAX_LFN + 1];
#endif
    uint32_t count = 0;

    if (path == NULL) {
        return SD_ERR_PARAM;
    }

#if _LFN_UNICODE
    path_to_tchar(path, tpath, sizeof(tpath) / sizeof(tpath[0]));
#else
    tpath = path;
#endif

    BSP_DEBUG_PRINTF("[SD] Listing directory: %s\r\n", path);

    fno.lfname = lfn_buf;
    fno.lfsize = sizeof(lfn_buf);

    fr = f_opendir(&dir, tpath);
    if (fr != FR_OK) {
        BSP_DEBUG_PRINTF("[SD] f_opendir(%s) failed: %d\r\n", path, fr);
        return SD_MapFRESULT(fr);
    }

    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) {
            break;
        }

        if (fno.fname[0] == '.') {
            continue;
        }

        count++;

#if _LFN_UNICODE
        const WCHAR *lfname  = fno.lfname;
        const WCHAR *sfname  = fno.fname;
        const char  *display;

        if (lfname && lfname[0]) {
            utf16_to_utf8(lfname, utf8_buf, sizeof(utf8_buf));
            display = utf8_buf;
        } else {
            utf16_to_utf8(sfname, utf8_buf, sizeof(utf8_buf));
            display = utf8_buf;
        }
#else
        const char *display = (fno.lfname && fno.lfname[0])
                            ? fno.lfname : fno.fname;
#endif

        if (fno.fattrib & AM_DIR) {
            BSP_DEBUG_PRINTF("  [DIR]  %s\r\n", display);
        } else {
            BSP_DEBUG_PRINTF("  [FILE] %s  %lu bytes\r\n", display, fno.fsize);
        }
    }

    f_closedir(&dir);
    BSP_DEBUG_PRINTF("[SD] Total: %lu entries\r\n", count);
    return SD_OK;
}

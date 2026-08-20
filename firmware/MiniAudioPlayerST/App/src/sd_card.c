/**
  ******************************************************************************
  * @file    sd_card.c
  * @brief   SD 卡应用层 — 基于 FatFs 的文件系统操作封装
  * @note    依赖 FATFS 层 (fatfs.h), 不直接调用 BSP_SD。
  *          当 _LFN_UNICODE=1 时内部将 char* 路径转为 TCHAR (WCHAR),
  *          LFN 为 UTF-16LE, 串口输出转 UTF-8。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "sd_card.h"
#include "bsp_config.h"

#define LOG_MODULE "SDCard"

#include <string.h>

/* 内部辅助 ----------------------------------------------------------------*/

/* 音频目录扫描共用 LFN 缓冲，当前裸机实现不支持并发目录扫描。 */
static WCHAR audio_lfn_buf[_MAX_LFN + 1];

/**
  * @brief  将 FatFs FRESULT 映射为应用层 SD_Status_t
  */
static SD_Status_t SD_MapFRESULT(FRESULT fr)
{
    switch (fr) {
        case FR_OK:
            return SD_OK;
        case FR_DISK_ERR:
        case FR_NOT_READY:
            return SD_ERR_MOUNT;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return SD_ERR_OPEN;
        case FR_NO_FILESYSTEM:
            return SD_ERR_NO_FILESYSTEM;
        default:
            return SD_ERR_READ;
    }
}

uint8_t SD_CountFiles(const WCHAR *path)
{
    DIR      dir;
    FILINFO  fno   = {0};
    uint32_t count = 0;

    if (f_opendir(&dir, path) != FR_OK) {
        return 0;
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
        if (!(fno.fattrib & AM_DIR)) {
            count++;
        }
    }
    f_closedir(&dir);
    return count;
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

static void wchar_ncpy(WCHAR *dst, const WCHAR *src, int max_chars)
{
    int i;
    for (i = 0; i < max_chars - 1 && src[i] != 0; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static WCHAR ascii_to_lower(WCHAR ch)
{
    if (ch >= (WCHAR)'A' && ch <= (WCHAR)'Z') {
        return ch + ((WCHAR)'a' - (WCHAR)'A');
    }
    return ch;
}

static uint8_t is_audio_file_name(const WCHAR *name)
{
    const WCHAR *extension = NULL;

    if (name == NULL) {
        return 0U;
    }

    while (*name != 0U) {
        if (*name == (WCHAR)'.') {
            extension = name;
        }
        name++;
    }

    if (extension == NULL) {
        return 0U;
    }

    if ((ascii_to_lower(extension[0]) == (WCHAR)'.')
        && (ascii_to_lower(extension[1]) == (WCHAR)'m')
        && (ascii_to_lower(extension[2]) == (WCHAR)'p')
        && (ascii_to_lower(extension[3]) == (WCHAR)'3')
        && (extension[4] == 0U)) {
        return 1U;
    }

    return 0U;
}

// 拼接文件路径，返回 1 表示成功，0 表示失败 (路径过长)
static uint8_t build_file_path(const WCHAR *directory,
                               const WCHAR *name,
                               WCHAR *path,
                               uint16_t path_capacity)
{
    uint16_t length = 0U;

    if ((directory == NULL) || (name == NULL)
        || (path == NULL) || (path_capacity == 0U)) {
        return 0U;
    }

    while ((*directory != 0U) && (length + 1U < path_capacity)) {
        path[length++] = *directory++;
    }

    // length + 1 >= path_capacity, 越界，说明路径过长，直接返回失败，并将 path[0] 置为 0
    if (*directory != 0U) {
        path[0] = 0U;
        return 0U;
    }

    if ((length > 0U) && (path[length - 1U] != (WCHAR)'/')) {
        if (length + 1U >= path_capacity) {
            path[0] = 0U;
            return 0U;
        }
        path[length++] = (WCHAR)'/';
    }

    while ((*name != 0U) && (length + 1U < path_capacity)) {
        path[length++] = *name++;
    }
    if (*name != 0U) {
        path[0] = 0U;
        return 0U;
    }

    path[length] = 0U;
    return 1U;
}

/*
 * UTF-16LE → UTF-8 转换 (BMP only, 覆盖所有 CJK)
 */
static void utf16_to_utf8(const WCHAR *src, char *dst, uint16_t dst_size)
{
    if (!src || !dst || dst_size == 0){
        return;
    }

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

    LOG_DEBUG("Mount", "Mounting file system");

    fr = f_mount(&USERFatFS, TUSERPath, 1);
    if (fr != FR_OK) {
        LOG_DEBUG("Mount", "f_mount failed: %d", fr);
        return SD_MapFRESULT(fr);
    }

    LOG_DEBUG("Mount", "OK");
    return SD_OK;
}

/**
  * @brief  卸载文件系统
  */
void SD_Unmount(void)
{
    f_mount(NULL, TUSERPath, 0);
    LOG_DEBUG("Unmount", "Done");
}

SD_Status_t SD_ReadFile(const WCHAR *path, WCHAR *buffer, uint32_t *len)
{
    FRESULT fr;
    FIL     file;
    UINT    bytes_read;

    if (path == NULL || buffer == NULL || len == NULL) {
        return SD_ERR_PARAM;
    }

    fr = f_open(&file, path, FA_READ | FA_OPEN_EXISTING);
    if (fr != FR_OK) {
        return SD_MapFRESULT(fr);
    }

    fr   = f_read(&file, buffer, *len, &bytes_read);
    *len = bytes_read;

    f_close(&file);

    if (fr != FR_OK) {
        return SD_MapFRESULT(fr);
    }
    return SD_OK;
}

/**
  * @brief  DEBUG FUNCTION 列出目录内容, 通过 LOG_DEBUG 输出到串口
  */
SD_Status_t SD_Debug_ListDir(const char *path)
{
    FRESULT fr;
    DIR     dir;
    FILINFO fno = {0};
#if _LFN_UNICODE
    TCHAR        tpath[_MAX_LFN + 1];    /* 栈: 512B */
    static WCHAR lfn_buf[_MAX_LFN + 1];  /* 静态: 512B, 不占栈 */
    static char  utf8_buf[_MAX_LFN + 1]; /* 静态: 256B, 不占栈 */
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

    LOG_DEBUG("ListDir", "path=%s", path);

    fno.lfname = lfn_buf;
    fno.lfsize = sizeof(lfn_buf) / sizeof(lfn_buf[0]);

    fr = f_opendir(&dir, tpath);
    if (fr != FR_OK) {
        LOG_DEBUG("ListDir", "f_opendir(%s) failed: %d", path, fr);
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
        const WCHAR *lfname = fno.lfname;
        const WCHAR *sfname = fno.fname;
        const char  *display;

        if (lfname && lfname[0]) {
            utf16_to_utf8(lfname, utf8_buf, sizeof(utf8_buf));
            display = utf8_buf;
        } else {
            utf16_to_utf8(sfname, utf8_buf, sizeof(utf8_buf));
            display = utf8_buf;
        }
#else
        const char *display = (fno.lfname && fno.lfname[0]) ? fno.lfname : fno.fname;
#endif

        if (fno.fattrib & AM_DIR) {
            LOG_DEBUG("DIR", "%s", display);
        } else {
            LOG_DEBUG("FILE", "%s  %lu bytes",
                             display,
                             (unsigned long)fno.fsize);
        }
    }

    f_closedir(&dir);
    LOG_DEBUG("ListDir", "Total: %lu entries",
                     (unsigned long)count);
    return SD_OK;
}

uint8_t SD_GetFileListWindow(WCHAR **list, uint8_t max_entries, uint8_t max_chars_per_entry,
                             uint32_t start_index, const WCHAR *path)
{
    if (list == NULL || path == NULL) {
        return 0;
    }

    FRESULT      fr;
    DIR          dir;
    FILINFO      fno = {0};
    static WCHAR lfn_buf[_MAX_LFN + 1]; /* 静态: 512B, 不占栈 */
    uint8_t      count   = 0;
    uint8_t      skipped = 0;

    fno.lfname = lfn_buf;
    fno.lfsize = sizeof(lfn_buf) / sizeof(lfn_buf[0]);

    fr = f_opendir(&dir, path);
    if (fr != FR_OK) {
        return 0;
    }

    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) {
            break;
        }

        if (fno.fname[0] == '.') {
            continue;
        }

        if (skipped < start_index) {
            skipped++;
            continue;
        }

        if (count >= max_entries) {
            break;
        }

        const WCHAR *src = (fno.lfname[0]) ? fno.lfname : fno.fname;
        wchar_ncpy(list[count], src, max_chars_per_entry);
        count++;
    }

    f_closedir(&dir);
    return count;
}

uint32_t SD_CountAudioFiles(const WCHAR *path)
{
    DIR dir;
    FILINFO fno = {0};
    uint32_t count = 0U;

    if (path == NULL) {
        return 0U;
    }

    fno.lfname = audio_lfn_buf;
    fno.lfsize = sizeof(audio_lfn_buf) / sizeof(audio_lfn_buf[0]);

    if (f_opendir(&dir, path) != FR_OK) {
        return 0U;
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0U) {
        const WCHAR *name;

        if ((fno.fattrib & AM_DIR) || (fno.fname[0] == (WCHAR)'.')) {
            continue;
        }

        name = (fno.lfname[0] != 0U) ? fno.lfname : fno.fname;
        if (is_audio_file_name(name)) {
            count++;
        }
    }

    f_closedir(&dir);
    return count;
}

uint8_t SD_GetAudioFileListWindow(WCHAR **list,
                                  uint8_t max_entries,
                                  uint16_t max_chars_per_entry,
                                  uint32_t start_index,
                                  const WCHAR *path)
{
    DIR dir;
    FILINFO fno = {0};
    uint32_t audio_index = 0U;
    uint8_t count = 0U;

    if ((list == NULL) || (path == NULL)
        || (max_entries == 0U) || (max_chars_per_entry == 0U)) {
        return 0U;
    }

    fno.lfname = audio_lfn_buf;
    fno.lfsize = sizeof(audio_lfn_buf) / sizeof(audio_lfn_buf[0]);

    if (f_opendir(&dir, path) != FR_OK) {
        return 0U;
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0U) {
        const WCHAR *name;

        if ((fno.fattrib & AM_DIR) || (fno.fname[0] == (WCHAR)'.')) {
            continue;
        }

        name = (fno.lfname[0] != 0U) ? fno.lfname : fno.fname;
        if (!is_audio_file_name(name)) {
            continue;
        }

        if (audio_index++ < start_index) {
            continue;
        }

        wchar_ncpy(list[count], name, max_chars_per_entry);
        count++;
        if (count >= max_entries) {
            break;
        }
    }

    f_closedir(&dir);
    return count;
}

uint8_t SD_GetAudioFilePathByIndex(const WCHAR *directory,
                                   uint32_t index,
                                   WCHAR *path,
                                   uint16_t path_capacity)
{
    DIR dir;
    FILINFO fno = {0};
    uint32_t audio_index = 0U;
    uint8_t result = 0U;

    if ((directory == NULL) || (path == NULL) || (path_capacity == 0U)) {
        return 0U;
    }
    path[0] = 0U;

    fno.lfname = audio_lfn_buf;
    fno.lfsize = sizeof(audio_lfn_buf) / sizeof(audio_lfn_buf[0]);

    if (f_opendir(&dir, directory) != FR_OK) {
        return 0U;
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0U) {
        const WCHAR *name;

        if ((fno.fattrib & AM_DIR) || (fno.fname[0] == (WCHAR)'.')) {
            continue;
        }

        name = (fno.lfname[0] != 0U) ? fno.lfname : fno.fname;
        if (!is_audio_file_name(name)) {
            continue;
        }

        if (audio_index == index) {
            result = build_file_path(directory,
                                     name,
                                     path,
                                     path_capacity);
            break;
        }
        audio_index++;
    }

    f_closedir(&dir);
    return result;
}

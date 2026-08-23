#include "file_manager.h"

// 显示窗口管理的状态机

#define MAX_FILE_NAME_LEN 127
#define LIST_COUNT        4
static const WCHAR MUSIC_PATH[] = {'/', 'm', 'u', 's', 'i', 'c', 0};

typedef struct
{
    uint32_t cursor_line;
    uint32_t top_line;
    uint32_t total_line;
    uint8_t  visible_line;
    WCHAR    file_list[LIST_COUNT][MAX_FILE_NAME_LEN];
    WCHAR   *file_list_ptr[LIST_COUNT];
} FileListView_t;

static FileListView_t file_view = {
    .cursor_line  = 0,
    .top_line     = 0,
    .total_line   = 0,
    .visible_line = 0,
};

static void _ClampTopToCursor(void)
{
    if (file_view.cursor_line < file_view.top_line)
    {
        file_view.top_line = file_view.cursor_line;
    }
    else if (file_view.cursor_line >= file_view.top_line + LIST_COUNT)
    {
        file_view.top_line = file_view.cursor_line - LIST_COUNT + 1;
    }
}

static void _RefreshWindow(void)
{
    file_view.visible_line = SD_GetAudioFileListWindow(
        file_view.file_list_ptr, LIST_COUNT, MAX_FILE_NAME_LEN, file_view.top_line, MUSIC_PATH);
}

static void _MoveCursor(int8_t delta)
{
    uint32_t old_top;

    if (file_view.total_line == 0)
    {
        return;
    }

    old_top = file_view.top_line;
    if (delta < 0)
    {
        file_view.cursor_line = (file_view.cursor_line == 0U) ? file_view.total_line - 1U
                                                              : file_view.cursor_line - 1U;
    }
    else
    {
        file_view.cursor_line++;
        if (file_view.cursor_line >= file_view.total_line)
        {
            file_view.cursor_line = 0U;
        }
    }
    _ClampTopToCursor();

    if (file_view.top_line != old_top)
    {
        _RefreshWindow();
    }
}

SD_Status_t FileManager_Init(void)
{
    SD_Status_t status = SD_Mount();

    if (status != SD_OK)
    {
        file_view.total_line   = 0U;
        file_view.visible_line = 0U;
        return status;
    }
    for (int i = 0; i < LIST_COUNT; i++)
    {
        file_view.file_list_ptr[i] = file_view.file_list[i];
    }
    file_view.total_line  = SD_CountAudioFiles(MUSIC_PATH);
    file_view.cursor_line = 0;
    file_view.top_line    = 0;
    _RefreshWindow();
    return SD_OK;
}

void FileManager_MoveCursorUp()
{
    _MoveCursor(-1);
}

void FileManager_MoveCursorDown()
{
    _MoveCursor(1);
}

uint8_t FileManager_GetVisibleCount()
{
    return file_view.visible_line;
}

uint8_t FileManager_GetCursorRow()
{
    return (uint8_t)(file_view.cursor_line - file_view.top_line);
}

const WCHAR *FileManager_GetVisibleEntry(uint8_t row)
{
    if (row >= LIST_COUNT)
    {
        return NULL;
    }

    return file_view.file_list[row];
}

uint32_t FileManager_GetFileCount(void)
{
    return file_view.total_line;
}

uint32_t FileManager_GetSelectedIndex(void)
{
    return file_view.cursor_line;
}

uint8_t FileManager_SelectIndex(uint32_t index)
{
    if (index >= file_view.total_line)
    {
        return 0U;
    }

    file_view.cursor_line = index;
    _ClampTopToCursor();
    _RefreshWindow();
    return 1U;
}

uint8_t FileManager_GetPath(uint32_t index, WCHAR *path, uint16_t path_capacity)
{
    if (index >= file_view.total_line)
    {
        return 0U;
    }

    return SD_GetAudioFilePathByIndex(MUSIC_PATH, index, path, path_capacity);
}

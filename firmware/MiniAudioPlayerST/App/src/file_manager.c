#include "file_manager.h"

// 显示窗口管理的状态机

#define MAX_FILE_NAME_LEN 127
#define LIST_COUNT        4
static const WCHAR MUSIC_PATH[] = {'/', 'm', 'u', 's', 'i', 'c', 0};

typedef struct
{
    uint8_t cursor_line;
    uint8_t top_line;
    uint8_t total_line;
    uint8_t visible_line;
    WCHAR   file_list[LIST_COUNT][MAX_FILE_NAME_LEN];
    WCHAR  *file_list_ptr[LIST_COUNT];
} FileListView_t;

static FileListView_t file_view = {
    .cursor_line  = 0,
    .top_line     = 0,
    .total_line   = 0,
    .visible_line = 0,
};

static void _ClampTopToCursor(void)
{
    if (file_view.cursor_line < file_view.top_line) {
        file_view.top_line = file_view.cursor_line;
    } else if (file_view.cursor_line >= file_view.top_line + LIST_COUNT) {
        file_view.top_line = file_view.cursor_line - LIST_COUNT + 1;
    }
}

static void _RefreshWindow(void)
{
    file_view.visible_line = SD_GetFileListWindow(
        file_view.file_list_ptr, LIST_COUNT, MAX_FILE_NAME_LEN, file_view.top_line, MUSIC_PATH);
}

static void _MoveCursor(int8_t delta)
{
    if (file_view.total_line == 0)
    {
        return;
    }

    int16_t next = (int16_t)file_view.cursor_line + delta;
    uint8_t old_top = file_view.top_line;

    if (next < 0) // 顶部越界，跳转到底部
    {
        next = file_view.total_line - 1;
    }
    else if (next >= file_view.total_line) { // 底部越界，跳转到顶部
        next = 0;
    }

    file_view.cursor_line = (int8_t)next;
    _ClampTopToCursor();

    if (file_view.top_line != old_top)
    {
        _RefreshWindow();
    }
}

void FileManager_Init()
{
    if (SD_Mount() != SD_OK) {
        return;
    }
    for (int i = 0; i < LIST_COUNT; i++) {
        file_view.file_list_ptr[i] = file_view.file_list[i];
    }
    file_view.total_line  = SD_CountFiles(MUSIC_PATH);
    file_view.cursor_line = 0;
    file_view.top_line    = 0;
    _RefreshWindow();
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

uint8_t FileManager_GetCursorRow(){
    return file_view.cursor_line - file_view.top_line;
}

const WCHAR* FileManager_GetVisibleEntry(uint8_t row)
{
    if (row >= LIST_COUNT)
    {
        return NULL;
    }

    return file_view.file_list[row];
}

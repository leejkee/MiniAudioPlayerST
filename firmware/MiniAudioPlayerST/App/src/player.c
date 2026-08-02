/**
  ******************************************************************************
  * @file    player.c
  * @brief   音频播放器应用层状态机实现
  * @note    FatFS 每次读取 512 字节，主循环每次最多发送 32 字节。
  ******************************************************************************
  */

#include "player.h"
#include "file_manager.h"
#include "mp3_metadata.h"
#include "bsp_vs1003.h"
#include <string.h>

#define PLAYER_FILE_BUFFER_SIZE       512U
#define PLAYER_PATH_CAPACITY          (_MAX_LFN + 16U)
#define PLAYER_DEFAULT_VOLUME         70U
#define PLAYER_VOLUME_ATTENUATION_MAX 80U
#define PLAYER_DREQ_POLL_TIMEOUT_MS   1U
#define PLAYER_END_FLUSH_BYTES        2048U

typedef struct
{
    player_state_t state; // 播放状态，停止，播放，暂停
    player_status_t last_error; // 最近一次错误
    player_mode_t mode; // 循环模式

    uint8_t initialized; // 是否初始化
    uint8_t file_open; // 文件是否打开
    uint8_t volume; // 音量
    uint8_t end_pending; // 歌曲结束之后的填充处理是否在进行

    FIL file;// FatFs文件句柄
    uint32_t current_index; // 当前歌曲索引
    uint32_t track_count; // 歌曲总数
    uint32_t file_size; // 文件大小
    uint32_t file_position; // 已经发送到vs1003的数据量

    uint8_t buffer[PLAYER_FILE_BUFFER_SIZE]; // 每次从sd卡最多读 512 byte
    uint16_t buffer_size; // 本次实际读取到的数据量
    uint16_t buffer_offset; // 这些数据已经向vs1003发送到什么位置
    uint16_t end_flush_remaining; // 文件传输结束，需要发送多少 byte 到vs1003来作为补发数据，确保播放完毕

    // 单曲播放状态
    uint32_t duration_seconds; // 当前歌曲总时长
    uint16_t elapsed_seconds; // 已经播放的时长
    uint32_t audio_data_size; // 排除 ID3v2/ID3v1 后的音频数据大小

    WCHAR current_path[PLAYER_PATH_CAPACITY]; // 当前歌曲的 unicode 路径
} player_context_t;

static player_context_t player;

static void Player_CloseFile(void)
{
    if (player.file_open != 0U) {
        (void)f_close(&player.file);
        player.file_open = 0U;
    }
}

static void Player_ResetTrack(void)
{
    Player_CloseFile();
    player.buffer_size = 0U;
    player.buffer_offset = 0U;
    player.file_position = 0U;
    player.end_pending = 0U;
    player.end_flush_remaining = 0U;
    player.duration_seconds = 0U;
    player.elapsed_seconds = 0U;
    player.audio_data_size = 0U;
}

static void Player_BeginEndOfTrack(void)
{
    Player_CloseFile();
    player.buffer_size = 0U;
    player.buffer_offset = 0U;
    player.end_pending = 1U;
    player.end_flush_remaining = PLAYER_END_FLUSH_BYTES;
}

static void Player_SetError(player_status_t error)
{
    Player_ResetTrack();
    player.last_error = error;
    player.state = PLAYER_STATE_ERROR;
}

static void Player_CopyPath(WCHAR *destination,
                            const WCHAR *source,
                            uint16_t capacity)
{
    uint16_t index = 0U;

    if ((destination == NULL) || (source == NULL) || (capacity == 0U)) {
        return;
    }

    while ((source[index] != 0U) && (index + 1U < capacity)) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = 0U;
}

static uint8_t Player_VolumeToAttenuation(uint8_t percent)
{
    if (percent == 0U) {
        return 0xFEU;
    }

    return (uint8_t)(((uint32_t)(100U - percent)
                      * PLAYER_VOLUME_ATTENUATION_MAX)
                     / 100U);
}

static player_status_t Player_ApplyVolume(uint8_t percent)
{
    uint8_t attenuation = Player_VolumeToAttenuation(percent);

    return (BSP_VS1003_SetVolume(attenuation, attenuation)
            == BSP_VS1003_OK)
        ? PLAYER_OK
        : PLAYER_ERR_DECODER;
}

static void Player_HandleEndOfTrack(void)
{
    player_status_t status;

    if (player.mode == PLAYER_MODE_REPEAT_ALL) {
        status = Player_Next();
    } else {
        status = Player_Play(player.current_index);
    }

    if ((status != PLAYER_OK) && (player.state != PLAYER_STATE_ERROR)) {
        Player_SetError(status);
    }
}

static void Player_PumpEndOfTrack(void)
{
    static const uint8_t zeros[BSP_VS1003_SDI_CHUNK_SIZE] = {0};
    uint16_t chunk;
    bsp_vs1003_status_t decoder_status;

    if (!BSP_VS1003_IsReady()) {
        return;
    }

    chunk = (player.end_flush_remaining > sizeof(zeros))
        ? sizeof(zeros)
        : player.end_flush_remaining;

    decoder_status = BSP_VS1003_SendData(
        zeros,
        chunk,
        PLAYER_DREQ_POLL_TIMEOUT_MS);
    if (decoder_status == BSP_VS1003_ERR_TIMEOUT) {
        return;
    }
    if (decoder_status != BSP_VS1003_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return;
    }

    player.end_flush_remaining -= chunk;
    if (player.end_flush_remaining == 0U) {
        Player_HandleEndOfTrack();
    }
}

player_status_t Player_Init(void)
{
    SD_Status_t sd_status;
    bsp_vs1003_status_t decoder_status;

    Player_CloseFile();
    memset(&player, 0, sizeof(player));
    player.state = PLAYER_STATE_UNINIT;
    player.mode = PLAYER_MODE_REPEAT_ALL;
    player.volume = PLAYER_DEFAULT_VOLUME;

    sd_status = FileManager_Init();
    if (sd_status != SD_OK) {
        Player_SetError(PLAYER_ERR_MOUNT);
        return PLAYER_ERR_MOUNT;
    }

    decoder_status = BSP_VS1003_Init();
    if (decoder_status != BSP_VS1003_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return PLAYER_ERR_DECODER;
    }

    player.initialized = 1U;
    player.track_count = FileManager_GetFileCount();
    player.current_index = FileManager_GetSelectedIndex();
    player.state = PLAYER_STATE_STOPPED;
    player.last_error = PLAYER_OK;

    if (Player_SetVolume(PLAYER_DEFAULT_VOLUME) != PLAYER_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return PLAYER_ERR_DECODER;
    }

    if (player.track_count == 0U) {
        player.last_error = PLAYER_ERR_NO_FILES;
        return PLAYER_ERR_NO_FILES;
    }

    return PLAYER_OK;
}

player_status_t Player_RefreshPlaylist(void)
{
    SD_Status_t status;

    if (player.initialized == 0U) {
        return PLAYER_ERR_NOT_INITIALIZED;
    }
    if ((player.state == PLAYER_STATE_PLAYING)
        || (player.state == PLAYER_STATE_PAUSED)) {
        return PLAYER_ERR_PARAM;
    }

    status = FileManager_Init();
    if (status != SD_OK) {
        Player_SetError(PLAYER_ERR_MOUNT);
        return PLAYER_ERR_MOUNT;
    }

    player.track_count = FileManager_GetFileCount();
    player.current_index = FileManager_GetSelectedIndex();
    player.current_path[0] = 0U;
    player.state = PLAYER_STATE_STOPPED;

    if (player.track_count == 0U) {
        player.last_error = PLAYER_ERR_NO_FILES;
        return PLAYER_ERR_NO_FILES;
    }

    player.last_error = PLAYER_OK;
    return PLAYER_OK;
}

player_status_t Player_Play(uint32_t track_index)
{
    static WCHAR path[PLAYER_PATH_CAPACITY];
    mp3_metadata_t metadata;
    bsp_vs1003_status_t decoder_status;
    FRESULT file_status;

    if (player.initialized == 0U) {
        return PLAYER_ERR_NOT_INITIALIZED;
    }

    if ((player.track_count == 0U) || (track_index >= player.track_count)) {
        return PLAYER_ERR_PARAM;
    }

    if (!FileManager_GetPath(track_index,
                             path,
                             sizeof(path) / sizeof(path[0]))) {
        return PLAYER_ERR_OPEN;
    }

    Player_ResetTrack();

    file_status = f_open(&player.file, path, FA_READ | FA_OPEN_EXISTING);
    if (file_status != FR_OK) {
        Player_SetError(PLAYER_ERR_OPEN);
        return PLAYER_ERR_OPEN;
    }
    player.file_open = 1U;

    player.file_size = (uint32_t)f_size(&player.file);
    MP3_MetadataRead(&player.file,
                     player.file_size,
                     player.buffer,
                     (uint16_t)sizeof(player.buffer),
                     &metadata);
    player.duration_seconds = metadata.duration_seconds;
    player.audio_data_size = metadata.audio_size;

    file_status = f_lseek(&player.file, metadata.audio_offset);
    if (file_status != FR_OK) {
        Player_SetError(PLAYER_ERR_READ);
        return PLAYER_ERR_READ;
    }

    decoder_status = BSP_VS1003_SoftReset();

    if (decoder_status != BSP_VS1003_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return PLAYER_ERR_DECODER;
    }

    if (Player_ApplyVolume(player.volume) != PLAYER_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return PLAYER_ERR_DECODER;
    }

    player.current_index = track_index;
    Player_CopyPath(player.current_path,
                    path,
                    sizeof(player.current_path)
                        / sizeof(player.current_path[0]));
    (void)FileManager_SelectIndex(track_index);

    player.state = PLAYER_STATE_PLAYING;
    player.last_error = PLAYER_OK;
    return PLAYER_OK;
}

player_status_t Player_PlaySelected(void)
{
    return Player_Play(FileManager_GetSelectedIndex());
}

void Player_Stop(void)
{
    if (player.initialized == 0U) {
        return;
    }

    Player_ResetTrack();

    if (BSP_VS1003_SoftReset() != BSP_VS1003_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return;
    }

    player.state = PLAYER_STATE_STOPPED;
    player.last_error = PLAYER_OK;
}

void Player_Pause(void)
{
    if (player.state == PLAYER_STATE_PLAYING) {
        player.state = PLAYER_STATE_PAUSED;
    }
}

void Player_Resume(void)
{
    if (player.state == PLAYER_STATE_PAUSED) {
        player.state = PLAYER_STATE_PLAYING;
    }
}

void Player_TogglePause(void)
{
    if (player.state == PLAYER_STATE_PLAYING) {
        Player_Pause();
    } else if (player.state == PLAYER_STATE_PAUSED) {
        Player_Resume();
    } else if ((player.state == PLAYER_STATE_STOPPED)
               && (player.track_count > 0U)) {
        (void)Player_Play(player.current_index);
    }
}

static player_status_t Player_PlayRelative(int8_t direction)
{
    uint32_t track_index;

    if (player.initialized == 0U) {
        return PLAYER_ERR_NOT_INITIALIZED;
    }
    if (player.track_count == 0U) {
        return PLAYER_ERR_NO_FILES;
    }

    if (direction > 0) {
        track_index = (player.current_index + 1U) % player.track_count;
    } else {
        track_index = (player.current_index == 0U)
            ? player.track_count - 1U
            : player.current_index - 1U;
    }

    return Player_Play(track_index);
}

player_status_t Player_Next(void)
{
    return Player_PlayRelative(1);
}

player_status_t Player_Previous(void)
{
    return Player_PlayRelative(-1);
}

player_status_t Player_SetVolume(uint8_t percent)
{
    if (percent > 100U) {
        return PLAYER_ERR_PARAM;
    }
    if (player.initialized == 0U) {
        return PLAYER_ERR_NOT_INITIALIZED;
    }

    if (Player_ApplyVolume(percent) != PLAYER_OK) {
        player.last_error = PLAYER_ERR_DECODER;
        return PLAYER_ERR_DECODER;
    }

    player.volume = percent;
    player.last_error = PLAYER_OK;
    return PLAYER_OK;
}

player_status_t Player_SetMode(player_mode_t mode)
{
    if (mode >= PLAYER_MODE_COUNT) {
        return PLAYER_ERR_PARAM;
    }

    player.mode = mode;
    return PLAYER_OK;
}

void Player_Tick(void)
{
    UINT bytes_read;
    FRESULT file_status;
    uint32_t stream_remaining;
    uint16_t remaining;
    uint16_t chunk;
    uint16_t read_size;
    bsp_vs1003_status_t decoder_status;

    if (player.state != PLAYER_STATE_PLAYING) {
        return;
    }
    if (player.end_pending != 0U) {
        Player_PumpEndOfTrack();
        return;
    }
    if (player.file_open == 0U) {
        Player_SetError(PLAYER_ERR_OPEN);
        return;
    }

    if (player.buffer_offset >= player.buffer_size) {
        stream_remaining = (player.audio_data_size > player.file_position)
            ? player.audio_data_size - player.file_position
            : 0U;
        if ((player.audio_data_size != 0U) && (stream_remaining == 0U)) {
            Player_BeginEndOfTrack();
            return;
        }

        read_size = sizeof(player.buffer);
        if ((player.audio_data_size != 0U) && (stream_remaining < read_size)) {
            read_size = (uint16_t)stream_remaining;
        }
        file_status = f_read(&player.file,
                             player.buffer,
                             read_size,
                             &bytes_read);
        if (file_status != FR_OK) {
            Player_SetError(PLAYER_ERR_READ);
            return;
        }
        if (bytes_read == 0U) {
            Player_BeginEndOfTrack();
            return;
        }

        player.buffer_size = (uint16_t)bytes_read;
        player.buffer_offset = 0U;
    }

    if (!BSP_VS1003_IsReady()) {
        return;
    }

    remaining = player.buffer_size - player.buffer_offset;
    chunk = (remaining > BSP_VS1003_SDI_CHUNK_SIZE)
        ? BSP_VS1003_SDI_CHUNK_SIZE
        : remaining;

    decoder_status = BSP_VS1003_SendData(
        &player.buffer[player.buffer_offset],
        chunk,
        PLAYER_DREQ_POLL_TIMEOUT_MS);

    if (decoder_status == BSP_VS1003_ERR_TIMEOUT) {
        return;
    }
    if (decoder_status != BSP_VS1003_OK) {
        Player_SetError(PLAYER_ERR_DECODER);
        return;
    }

    player.buffer_offset += chunk;
    player.file_position += chunk;
}

player_state_t Player_GetState(void)
{
    return player.state;
}

player_status_t Player_GetLastError(void)
{
    return player.last_error;
}

player_mode_t Player_GetMode(void)
{
    return player.mode;
}

uint8_t Player_GetVolume(void)
{
    return player.volume;
}

uint32_t Player_GetCurrentIndex(void)
{
    return player.current_index;
}

uint32_t Player_GetTrackCount(void)
{
    return player.track_count;
}

uint32_t Player_GetFilePosition(void)
{
    return player.file_position;
}

uint32_t Player_GetFileSize(void)
{
    return player.file_size;
}

uint8_t Player_GetProgressPercent(void)
{
    uint32_t progress;
    uint32_t total_size = (player.audio_data_size != 0U)
        ? player.audio_data_size
        : player.file_size;

    if (total_size == 0U) {
        return 0U;
    }

    progress = (uint32_t)(((uint64_t)player.file_position * 100U)
                          / total_size);
    return (progress > 100U) ? 100U : (uint8_t)progress;
}

const WCHAR *Player_GetCurrentPath(void)
{
    return (player.current_path[0] == 0U)
        ? NULL
        : player.current_path;
}

uint32_t Player_GetDurationSeconds(void)
{
    return player.duration_seconds;
}

uint32_t Player_GetElapsedSeconds(void)
{
    uint16_t seconds;

    if ((player.state != PLAYER_STATE_PLAYING)
        && (player.state != PLAYER_STATE_PAUSED)) {
        return player.elapsed_seconds;
    }

    if (BSP_VS1003_ReadRegister(BSP_VS1003_REG_DECODE_TIME,
                                &seconds) == BSP_VS1003_OK) {
        player.elapsed_seconds = seconds;
    }

    return player.elapsed_seconds;
}

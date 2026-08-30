/**
  ******************************************************************************
  * @file    player.c
  * @brief   音频播放器应用层状态机实现
  * @note    Player 管理播放策略，文件流读取和 VS1003 状态由 Decoder 管理。
  ******************************************************************************
  */

#include "player.h"
#include "decoder.h"
#include "file_manager.h"
#include "mp3_metadata.h"

#include <string.h>

#define PLAYER_DEFAULT_VOLUME       70U
#define PLAYER_METADATA_BUFFER_SIZE 512U

typedef struct
{
    player_state_t  state;
    player_status_t last_error;
    player_mode_t   mode;

    uint8_t         initialized;
    uint8_t         volume;
    uint32_t        file_size;
    uint32_t        duration_seconds; // 歌曲时长
    uint32_t        audio_data_size;
    uint8_t         metadata_buffer[PLAYER_METADATA_BUFFER_SIZE];
} player_context_t;

static player_context_t player;

static player_status_t  Player_MapDecoderStatus(decoder_status_t status)
{
    switch (status)
    {
        case DECODER_OK:
            return PLAYER_OK;
        case DECODER_ERR_PARAM:
            return PLAYER_ERR_PARAM;
        case DECODER_ERR_FILE:
            return PLAYER_ERR_READ;
        case DECODER_ERR_NOT_INITIALIZED:
        case DECODER_ERR_DEVICE:
        default:
            return PLAYER_ERR_DECODER;
    }
}

static void Player_ResetTrack(void)
{
    Decoder_Abort();
    FileManager_CloseCurrent();
    player.duration_seconds = 0U;
    player.audio_data_size  = 0U;
}

static void Player_SetError(player_status_t error)
{
    Player_ResetTrack();
    player.last_error = error;
    player.state      = PLAYER_STATE_ERROR;
}

static void Player_HandleEndOfTrack(void)
{
    player_status_t status;

    if (player.mode == PLAYER_MODE_REPEAT_ONE)
    {
        status = Player_Play(Player_GetCurrentIndex());
    }
    else
    {
        status = Player_Next();
    }

    if ((status != PLAYER_OK) && (player.state != PLAYER_STATE_ERROR))
    {
        Player_SetError(status);
    }
}

player_status_t Player_Init(void)
{
    SD_Status_t      file_status;
    decoder_status_t decoder_status;

    Decoder_Abort();
    FileManager_CloseCurrent();
    memset(&player, 0, sizeof(player));
    player.state  = PLAYER_STATE_UNINIT;
    player.mode   = PLAYER_MODE_REPEAT_ALL;
    player.volume = PLAYER_DEFAULT_VOLUME;

    file_status = FileManager_Init();
    if (file_status != SD_OK)
    {
        Player_SetError(PLAYER_ERR_MOUNT);
        return PLAYER_ERR_MOUNT;
    }
    FileManager_ClearCurrent();

    decoder_status = Decoder_Init();
    if (decoder_status != DECODER_OK)
    {
        Player_SetError(PLAYER_ERR_DECODER);
        return PLAYER_ERR_DECODER;
    }

    player.initialized = 1U;
    player.state       = PLAYER_STATE_STOPPED;
    player.last_error  = PLAYER_OK;

    if (Player_SetVolume(PLAYER_DEFAULT_VOLUME) != PLAYER_OK)
    {
        Player_SetError(PLAYER_ERR_DECODER);
        return PLAYER_ERR_DECODER;
    }

    if (FileManager_GetFileCount() == 0U)
    {
        player.last_error = PLAYER_ERR_NO_FILES;
        return PLAYER_ERR_NO_FILES;
    }

    return PLAYER_OK;
}

// no refer
player_status_t Player_RefreshPlaylist(void)
{
    SD_Status_t status;

    if (player.initialized == 0U)
    {
        return PLAYER_ERR_NOT_INITIALIZED;
    }
    if ((player.state == PLAYER_STATE_PLAYING) || (player.state == PLAYER_STATE_PAUSED))
    {
        return PLAYER_ERR_PARAM;
    }

    FileManager_CloseCurrent();
    status = FileManager_Init();
    if (status != SD_OK)
    {
        Player_SetError(PLAYER_ERR_MOUNT);
        return PLAYER_ERR_MOUNT;
    }

    FileManager_ClearCurrent();
    player.state = PLAYER_STATE_STOPPED;

    if (FileManager_GetFileCount() == 0U)
    {
        player.last_error = PLAYER_ERR_NO_FILES;
        return PLAYER_ERR_NO_FILES;
    }

    player.last_error = PLAYER_OK;
    return PLAYER_OK;
}

player_status_t Player_Play(uint32_t track_index)
{
    FIL             *file;
    mp3_metadata_t   metadata;
    SD_Status_t      file_status;
    decoder_status_t decoder_status;
    player_status_t  player_status;

    if (player.initialized == 0U)
    {
        return PLAYER_ERR_NOT_INITIALIZED;
    }
    if ((FileManager_GetFileCount() == 0U) || (track_index >= FileManager_GetFileCount()))
    {
        return PLAYER_ERR_PARAM;
    }

    Player_ResetTrack();
    file_status = FileManager_Open(track_index, &file);
    if (file_status != SD_OK)
    {
        Player_SetError(PLAYER_ERR_OPEN);
        return PLAYER_ERR_OPEN;
    }

    player.file_size = FileManager_GetCurrentFileSize();
    MP3_MetadataRead(file, player.file_size, player.metadata_buffer,
                     (uint16_t)sizeof(player.metadata_buffer), &metadata);

    decoder_status = Decoder_Start(file, metadata.audio_offset, metadata.audio_size);
    if (decoder_status != DECODER_OK)
    {
        player_status = Player_MapDecoderStatus(decoder_status);
        Player_SetError(player_status);
        return player_status;
    }

    player.duration_seconds = metadata.duration_seconds;
    player.audio_data_size  = metadata.audio_size;
    player.state            = PLAYER_STATE_PLAYING;
    player.last_error       = PLAYER_OK;
    return PLAYER_OK;
}

player_status_t Player_PlaySelected(void)
{
    return Player_Play(FileManager_GetSelectedIndex());
}

void Player_Stop(void)
{
    decoder_status_t status;

    if (player.initialized == 0U)
    {
        return;
    }

    status = Decoder_Stop();
    FileManager_CloseCurrent();
    player.duration_seconds = 0U;
    player.audio_data_size  = 0U;

    if (status != DECODER_OK)
    {
        Player_SetError(Player_MapDecoderStatus(status));
        return;
    }

    player.state      = PLAYER_STATE_STOPPED;
    player.last_error = PLAYER_OK;
}

void Player_Pause(void)
{
    if (player.state == PLAYER_STATE_PLAYING)
    {
        if (Decoder_Pause() != DECODER_OK)
        {
            Player_SetError(PLAYER_ERR_DECODER);
            return;
        }
        player.state = PLAYER_STATE_PAUSED;
    }
}

void Player_Resume(void)
{
    if (player.state == PLAYER_STATE_PAUSED)
    {
        if (Decoder_Resume() != DECODER_OK)
        {
            Player_SetError(PLAYER_ERR_DECODER);
            return;
        }
        player.state = PLAYER_STATE_PLAYING;
    }
}

void Player_TogglePause(void)
{
    if (player.state == PLAYER_STATE_PLAYING)
    {
        Player_Pause();
    }
    else if (player.state == PLAYER_STATE_PAUSED)
    {
        Player_Resume();
    }
    else if ((player.state == PLAYER_STATE_STOPPED) && (FileManager_GetFileCount() > 0U))
    {
        (void)Player_Play(Player_GetCurrentIndex());
    }
}

static player_status_t Player_PlayRelative(int8_t direction)
{
    uint32_t track_index;

    if (player.initialized == 0U)
    {
        return PLAYER_ERR_NOT_INITIALIZED;
    }

    if (!FileManager_GetAdjacentIndex(direction, &track_index))
    {
        return PLAYER_ERR_NO_FILES;
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
    decoder_status_t status;

    if (percent > 100U)
    {
        return PLAYER_ERR_PARAM;
    }
    if (player.initialized == 0U)
    {
        return PLAYER_ERR_NOT_INITIALIZED;
    }

    status = Decoder_SetVolume(percent);
    if (status != DECODER_OK)
    {
        player.last_error = PLAYER_ERR_DECODER;
        return Player_MapDecoderStatus(status);
    }

    player.volume     = percent;
    player.last_error = PLAYER_OK;
    return PLAYER_OK;
}

player_status_t Player_SetMode(player_mode_t mode)
{
    if (mode >= PLAYER_MODE_COUNT)
    {
        return PLAYER_ERR_PARAM;
    }

    player.mode = mode;
    return PLAYER_OK;
}

void Player_Tick(void)
{
    decoder_event_t event;

    if (player.state != PLAYER_STATE_PLAYING)
    {
        return;
    }

    event = Decoder_Tick();
    switch (event)
    {
        case DECODER_EVENT_NONE:
            break;
        case DECODER_EVENT_FINISHED:
            Player_HandleEndOfTrack();
            break;
        case DECODER_EVENT_FILE_ERROR:
            Player_SetError(PLAYER_ERR_READ);
            break;
        case DECODER_EVENT_DEVICE_ERROR:
        default:
            Player_SetError(PLAYER_ERR_DECODER);
            break;
    }
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
    return FileManager_GetCurrentIndex();
}

uint32_t Player_GetTrackCount(void)
{
    return FileManager_GetFileCount();
}

uint32_t Player_GetFilePosition(void)
{
    return Decoder_GetTransferredBytes();
}

uint32_t Player_GetFileSize(void)
{
    return player.file_size;
}

uint8_t Player_GetProgressPercent(void)
{
    uint32_t progress;
    uint32_t total_size = (player.audio_data_size != 0U) ? player.audio_data_size
                                                         : player.file_size;

    if (total_size == 0U)
    {
        return 0U;
    }

    progress = (uint32_t)(((uint64_t)Player_GetFilePosition() * 100U) / total_size);
    return (progress > 100U) ? 100U : (uint8_t)progress;
}

const WCHAR *Player_GetCurrentPath(void)
{
    return FileManager_GetCurrentPath();
}

uint32_t Player_GetDurationSeconds(void)
{
    return player.duration_seconds;
}

uint32_t Player_GetElapsedSeconds(void)
{
    return Decoder_GetElapsedSeconds();
}

/**
  ******************************************************************************
  * @file    player.h
  * @brief   音频播放器应用层状态机
  * @note    当前版本使用主循环轮询，不依赖 RTOS 和 DMA。
  ******************************************************************************
  */

#ifndef __PLAYER_H
#define __PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fatfs.h"

typedef enum
{
    PLAYER_STATE_UNINIT = 0,
    PLAYER_STATE_STOPPED,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_ERROR
} player_state_t;

typedef enum
{
    PLAYER_MODE_REPEAT_ALL = 0,
    PLAYER_MODE_REPEAT_ONE,
    PLAYER_MODE_COUNT
} player_mode_t;

typedef enum
{
    PLAYER_OK = 0,
    PLAYER_ERR_PARAM,
    PLAYER_ERR_NOT_INITIALIZED,
    PLAYER_ERR_MOUNT,
    PLAYER_ERR_NO_FILES,
    PLAYER_ERR_OPEN,
    PLAYER_ERR_READ,
    PLAYER_ERR_DECODER
} player_status_t;

/**
  * @brief  初始化文件列表和 VS1003
  * @retval PLAYER_ERR_NO_FILES 表示初始化成功但 /music 中没有 MP3。
  */
player_status_t Player_Init(void);

/**
  * @brief  主循环轮询入口
  * @note   每次最多向 VS1003 发送 32 字节，不会持续占用主循环。
  */
void Player_Tick(void);

/**
  * @brief  重新扫描 /music 音频文件列表
  */
player_status_t Player_RefreshPlaylist(void);

/**
  * @brief  播放指定索引的曲目
  */
player_status_t Player_Play(uint32_t track_index);

/**
  * @brief  播放文件列表当前选中的曲目
  */
player_status_t Player_PlaySelected(void);

void Player_Stop(void);
void Player_Pause(void);
void Player_Resume(void);
void Player_TogglePause(void);

player_status_t Player_Next(void);
player_status_t Player_Previous(void);

/**
  * @brief  设置音量百分比
  * @param  percent 0 为静音，100 为最大音量
  */
player_status_t Player_SetVolume(uint8_t percent);

player_status_t Player_SetMode(player_mode_t mode);

player_state_t Player_GetState(void);
player_status_t Player_GetLastError(void);
player_mode_t Player_GetMode(void);
uint8_t Player_GetVolume(void);
uint32_t Player_GetCurrentIndex(void);
uint32_t Player_GetTrackCount(void);
uint32_t Player_GetFilePosition(void);
uint32_t Player_GetFileSize(void);
uint8_t Player_GetProgressPercent(void);
const WCHAR *Player_GetCurrentPath(void);

/**
  * @brief  获取当前歌曲总时长
  * @retval 秒；ID3 TLEN 缺失且无法确认 CBR 时返回 0
  */
uint32_t Player_GetDurationSeconds(void);

/**
  * @brief  从 VS1003 解码时间寄存器获取当前已播放时长
  * @retval 秒；寄存器读取失败时返回最近一次成功读取的值
  */
uint32_t Player_GetElapsedSeconds(void);

#ifdef __cplusplus
}
#endif

#endif /* __PLAYER_H */

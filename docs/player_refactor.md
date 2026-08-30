# 应用层播放模块重构

## 目标

- FileManager 统一管理播放列表、当前文件路径和 `FIL` 生命周期。
- Decoder 接收 FileManager 持有的 `FIL *`，负责文件读取、流缓冲提交、VS1003 控制和曲末排空。
- Player 只保留面向用户的状态、播放模式、切歌策略和错误映射。
- `App_Run()` 继续只调度 `Player_Tick()` 和 `UI_Render_Tick()`。

## 模块关系

```text
App_Run
  -> Player_Tick
       -> Decoder_Tick
            -> f_read(FIL *)
            -> BSP_VS1003 流缓冲
       <- FINISHED / FILE_ERROR / DEVICE_ERROR
       -> Player 按 PlayMode 选择重播或下一曲
```

Decoder 从 Player 视角隐藏双缓冲的申请和提交，但中断安全的
`EMPTY -> WRITING -> READY -> ACTIVE` 状态机仍由 `BSP_VS1003` 维护。

## FileManager

FileManager 内部保存一个 `FIL current_file`，通过以下接口管理：

```c
SD_Status_t  FileManager_Open(uint32_t index, FIL **file);
void         FileManager_CloseCurrent(void);
FIL         *FileManager_GetCurrentFile(void);
uint32_t     FileManager_GetCurrentIndex(void);
uint8_t      FileManager_GetAdjacentIndex(int8_t direction, uint32_t *index);
uint32_t     FileManager_GetCurrentFileSize(void);
const WCHAR *FileManager_GetCurrentPath(void);
```

`FileManager_Open()` 返回的是借用指针，不是 `FIL` 副本。Decoder 停止之前，
FileManager 不得关闭或替换该文件。

## Decoder

Decoder 的内部状态为：

```text
UNINIT -> IDLE -> RUNNING -> DRAINING -> IDLE
                    |           |
                    +-> PAUSED <-+
                    |
                    +-> ERROR
```

- `Decoder_Start(file, audio_offset, audio_size)` 重新定位文件、复位 VS1003 并开始播放。
- `Decoder_Tick()` 每次最多执行一次 `f_read()` 和一次缓冲提交。
- 有效音频数据结束后进入 `DRAINING`，提交 2048 字节尾部填充。
- 填充完成且 BSP 流空闲后，返回 `DECODER_EVENT_FINISHED`。
- Decoder 不关闭 `FIL`，不选择上一曲或下一曲。

## Player

`Player_Play()` 的执行顺序：

```text
Decoder_Abort
  -> FileManager_CloseCurrent
  -> FileManager_Open
  -> MP3_MetadataRead
  -> Decoder_Start
  -> PLAYER_STATE_PLAYING
```

`Player_Tick()` 不再直接读取文件或调用 VS1003 BSP，只处理 Decoder 事件：

- `DECODER_EVENT_FINISHED`：单曲循环重播当前索引，列表循环调用 `Player_Next()`。
- `DECODER_EVENT_FILE_ERROR`：转换为 `PLAYER_ERR_READ`。
- `DECODER_EVENT_DEVICE_ERROR`：转换为 `PLAYER_ERR_DECODER`。

手动 `Player_Next()` / `Player_Previous()` 始终切换曲目；PlayMode 只决定自然播放结束后的动作。

## 所有权约束

切歌、停止或错误处理必须遵循：

```text
先 Decoder_Abort/Stop
-> 再 FileManager_CloseCurrent
```

这保证 Decoder 和 VS1003 DMA 不会继续访问已经关闭的 `FIL`。

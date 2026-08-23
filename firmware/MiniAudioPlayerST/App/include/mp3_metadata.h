#ifndef MP3_METADATA_H
#define MP3_METADATA_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "fatfs.h"

typedef struct
{
    uint32_t duration_seconds;
    uint32_t audio_offset;
    uint32_t audio_size;
} mp3_metadata_t;

/**
  * @brief  读取 MP3 音频范围和总时长
  * @note   优先采用 ID3v2.3/v2.4 TLEN；缺失时按已确认的 CBR 计算。
  * @param  scratch      调用方提供的临时扫描缓冲区
  * @param  scratch_size 临时缓冲区大小，至少 4 字节
  */
void MP3_MetadataRead(FIL *file, uint32_t file_size, uint8_t *scratch, uint16_t scratch_size,
                      mp3_metadata_t *metadata);

#ifdef __cplusplus
}
#endif

#endif /* MP3_METADATA_H */

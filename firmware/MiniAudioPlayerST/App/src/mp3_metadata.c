#include "mp3_metadata.h"

#include <string.h>

#define MP3_ID3V2_HEADER_SIZE  10U
#define MP3_ID3V1_TAG_SIZE     128U
#define MP3_FRAME_HEADER_SIZE  4U
#define MP3_FRAME_SEARCH_LIMIT (64U * 1024U)
#define MP3_TLEN_READ_LIMIT    32U

typedef struct
{
    uint32_t bitrate_bps;
    uint32_t sample_rate_hz;
    uint16_t frame_size;
    uint8_t version_id;
} mp3_frame_info_t;

static uint32_t MP3_ReadBE32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24)
         | ((uint32_t)data[1] << 16)
         | ((uint32_t)data[2] << 8)
         | data[3];
}

static uint32_t MP3_ReadSyncSafe32(const uint8_t *data)
{
    if (((data[0] | data[1] | data[2] | data[3]) & 0x80U) != 0U) {
        return UINT32_MAX;
    }

    return ((uint32_t)data[0] << 21)
         | ((uint32_t)data[1] << 14)
         | ((uint32_t)data[2] << 7)
         | data[3];
}

static uint8_t MP3_ReadAt(FIL *file,
                          uint32_t offset,
                          uint8_t *data,
                          uint16_t length)
{
    UINT bytes_read = 0U;

    if ((f_lseek(file, offset) != FR_OK)
        || (f_read(file, data, length, &bytes_read) != FR_OK)) {
        return 0U;
    }

    return (bytes_read == length) ? 1U : 0U;
}

static uint8_t MP3_IsFrameId(const uint8_t *id)
{
    uint8_t index;

    for (index = 0U; index < 4U; index++) {
        if (!(((id[index] >= 'A') && (id[index] <= 'Z'))
              || ((id[index] >= '0') && (id[index] <= '9')))) {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t MP3_ParseTlenText(const uint8_t *data,
                                 uint16_t length,
                                 uint32_t *duration_seconds)
{
    uint32_t milliseconds = 0U;
    uint16_t offset = 1U;
    uint8_t digit_found = 0U;
    uint8_t encoding;
    uint8_t little_endian = 0U;

    if ((data == NULL) || (duration_seconds == NULL) || (length < 2U)) {
        return 0U;
    }

    encoding = data[0];
    if (encoding == 1U) {
        if (length < 4U) {
            return 0U;
        }
        if ((data[1] == 0xFFU) && (data[2] == 0xFEU)) {
            little_endian = 1U;
        } else if (!((data[1] == 0xFEU) && (data[2] == 0xFFU))) {
            return 0U;
        }
        offset = 3U;
    } else if ((encoding != 0U) && (encoding != 2U) && (encoding != 3U)) {
        return 0U;
    }

    while (offset < length) {
        uint16_t character;

        if ((encoding == 1U) || (encoding == 2U)) {
            if (offset + 1U >= length) {
                break;
            }
            character = little_endian
                ? ((uint16_t)data[offset + 1U] << 8) | data[offset]
                : ((uint16_t)data[offset] << 8) | data[offset + 1U];
            offset += 2U;
        } else {
            character = data[offset++];
        }

        if (character == 0U) {
            break;
        }
        if ((character < '0') || (character > '9')
            || (milliseconds > (UINT32_MAX - (character - '0')) / 10U)) {
            return 0U;
        }
        milliseconds = milliseconds * 10U + (character - '0');
        digit_found = 1U;
    }

    if ((digit_found == 0U) || (milliseconds == 0U)) {
        return 0U;
    }

    *duration_seconds = milliseconds / 1000U;
    if ((milliseconds % 1000U) >= 500U) {
        (*duration_seconds)++;
    }
    return 1U;
}

static uint8_t MP3_ReadId3v2(FIL *file,
                             uint32_t file_size,
                             uint32_t *tag_end,
                             uint32_t *duration_seconds)
{
    uint8_t header[MP3_ID3V2_HEADER_SIZE];
    uint8_t frame_header[10];
    uint8_t text[MP3_TLEN_READ_LIMIT];
    uint32_t tag_data_size;
    uint32_t frames_end;
    uint32_t frame_offset;
    uint32_t frame_size;
    uint32_t extended_size;
    uint16_t text_size;
    uint8_t version;
    uint8_t flags;

    *tag_end = 0U;
    *duration_seconds = 0U;
    if ((file_size < sizeof(header))
        || (MP3_ReadAt(file, 0U, header, sizeof(header)) == 0U)
        || (header[0] != 'I') || (header[1] != 'D') || (header[2] != '3')) {
        return 0U;
    }

    version = header[3];
    flags = header[5];
    tag_data_size = MP3_ReadSyncSafe32(&header[6]);
    if ((tag_data_size == UINT32_MAX)
        || (tag_data_size > file_size - sizeof(header))) {
        return 0U;
    }

    frames_end = sizeof(header) + tag_data_size;
    *tag_end = frames_end;
    if ((version == 4U) && ((flags & 0x10U) != 0U)) {
        if (*tag_end > file_size - 10U) {
            *tag_end = 0U;
            return 0U;
        }
        *tag_end += 10U;
    }

    if ((version != 3U) && (version != 4U)) {
        return 1U;
    }

    frame_offset = sizeof(header);
    if ((flags & 0x40U) != 0U) {
        if ((frame_offset > frames_end - 4U)
            || (MP3_ReadAt(file, frame_offset, frame_header, 4U) == 0U)) {
            return 1U;
        }
        extended_size = (version == 4U)
            ? MP3_ReadSyncSafe32(frame_header)
            : MP3_ReadBE32(frame_header) + 4U;
        if ((extended_size == UINT32_MAX) || (extended_size < 4U)
            || (extended_size > frames_end - frame_offset)) {
            return 1U;
        }
        frame_offset += extended_size;
    }

    while (frame_offset <= frames_end - sizeof(frame_header)) {
        if ((MP3_ReadAt(file, frame_offset, frame_header,
                        sizeof(frame_header)) == 0U)
            || (frame_header[0] == 0U)
            || (MP3_IsFrameId(frame_header) == 0U)) {
            break;
        }

        frame_size = (version == 4U)
            ? MP3_ReadSyncSafe32(&frame_header[4])
            : MP3_ReadBE32(&frame_header[4]);
        if ((frame_size == UINT32_MAX)
            || (frame_size > frames_end - frame_offset - sizeof(frame_header))) {
            break;
        }

        if ((frame_header[0] == 'T') && (frame_header[1] == 'L')
            && (frame_header[2] == 'E') && (frame_header[3] == 'N')
            && (frame_header[9] == 0U)) {
            text_size = (frame_size > sizeof(text))
                ? sizeof(text)
                : (uint16_t)frame_size;
            if ((text_size > 0U)
                && (MP3_ReadAt(file,
                               frame_offset + sizeof(frame_header),
                               text,
                               text_size) != 0U)
                && (MP3_ParseTlenText(text, text_size,
                                      duration_seconds) != 0U)) {
                return 1U;
            }
        }
        frame_offset += sizeof(frame_header) + frame_size;
    }

    return 1U;
}

static uint8_t MP3_ParseFrameHeader(const uint8_t *data,
                                    mp3_frame_info_t *frame)
{
    static const uint16_t bitrate_mpeg1_l3[16] = {
        0U, 32U, 40U, 48U, 56U, 64U, 80U, 96U,
        112U, 128U, 160U, 192U, 224U, 256U, 320U, 0U
    };
    static const uint16_t bitrate_mpeg2_l3[16] = {
        0U, 8U, 16U, 24U, 32U, 40U, 48U, 56U,
        64U, 80U, 96U, 112U, 128U, 144U, 160U, 0U
    };
    static const uint32_t sample_rates[3][3] = {
        {11025U, 12000U, 8000U},
        {22050U, 24000U, 16000U},
        {44100U, 48000U, 32000U}
    };
    uint32_t header = MP3_ReadBE32(data);
    uint32_t coefficient;
    uint16_t bitrate_kbps;
    uint8_t version_id;
    uint8_t version_table;
    uint8_t bitrate_index;
    uint8_t sample_rate_index;
    uint8_t padding;

    if (((header & 0xFFE00000UL) != 0xFFE00000UL)
        || (((header >> 17) & 0x03U) != 0x01U)) {
        return 0U;
    }

    version_id = (uint8_t)((header >> 19) & 0x03U);
    bitrate_index = (uint8_t)((header >> 12) & 0x0FU);
    sample_rate_index = (uint8_t)((header >> 10) & 0x03U);
    padding = (uint8_t)((header >> 9) & 0x01U);
    if ((version_id == 1U) || (bitrate_index == 0U)
        || (bitrate_index == 15U) || (sample_rate_index == 3U)) {
        return 0U;
    }

    if (version_id == 3U) {
        version_table = 2U;
        bitrate_kbps = bitrate_mpeg1_l3[bitrate_index];
        coefficient = 144000U;
    } else {
        version_table = (version_id == 2U) ? 1U : 0U;
        bitrate_kbps = bitrate_mpeg2_l3[bitrate_index];
        coefficient = 72000U;
    }

    frame->sample_rate_hz = sample_rates[version_table][sample_rate_index];
    frame->bitrate_bps = (uint32_t)bitrate_kbps * 1000U;
    frame->frame_size = (uint16_t)(
        (coefficient * bitrate_kbps) / frame->sample_rate_hz + padding);
    frame->version_id = version_id;
    return (frame->frame_size >= MP3_FRAME_HEADER_SIZE) ? 1U : 0U;
}

static uint8_t MP3_ReadFrameAt(FIL *file,
                               uint32_t offset,
                               mp3_frame_info_t *frame)
{
    uint8_t header[MP3_FRAME_HEADER_SIZE];

    return ((MP3_ReadAt(file, offset, header, sizeof(header)) != 0U)
            && (MP3_ParseFrameHeader(header, frame) != 0U))
        ? 1U
        : 0U;
}

static uint8_t MP3_FindFirstFrame(FIL *file,
                                  uint32_t search_start,
                                  uint32_t audio_end,
                                  uint8_t *scratch,
                                  uint16_t scratch_size,
                                  uint32_t *frame_offset,
                                  mp3_frame_info_t *first_frame,
                                  uint8_t *is_cbr)
{
    mp3_frame_info_t candidate;
    mp3_frame_info_t next;
    mp3_frame_info_t third;
    UINT bytes_read;
    uint32_t search_end;
    uint32_t offset;
    uint32_t index;
    uint32_t candidate_offset;
    uint32_t next_offset;
    uint32_t third_offset;
    uint16_t chunk_size;

    if ((scratch == NULL) || (scratch_size < MP3_FRAME_HEADER_SIZE)
        || (search_start >= audio_end)) {
        return 0U;
    }

    search_end = search_start + MP3_FRAME_SEARCH_LIMIT;
    if ((search_end < search_start) || (search_end > audio_end)) {
        search_end = audio_end;
    }
    offset = search_start;

    while (offset < search_end) {
        chunk_size = (search_end - offset > scratch_size)
            ? scratch_size
            : (uint16_t)(search_end - offset);
        if ((f_lseek(file, offset) != FR_OK)
            || (f_read(file, scratch, chunk_size, &bytes_read) != FR_OK)
            || (bytes_read < MP3_FRAME_HEADER_SIZE)) {
            return 0U;
        }

        for (index = 0U;
             index + MP3_FRAME_HEADER_SIZE <= bytes_read;
             index++) {
            if (MP3_ParseFrameHeader(&scratch[index], &candidate) == 0U) {
                continue;
            }
            candidate_offset = offset + index;
            if (candidate.frame_size > audio_end - candidate_offset) {
                continue;
            }
            next_offset = candidate_offset + candidate.frame_size;
            if ((next_offset >= audio_end)
                || (MP3_ReadFrameAt(file, next_offset, &next) == 0U)
                || (next.version_id != candidate.version_id)
                || (next.sample_rate_hz != candidate.sample_rate_hz)) {
                continue;
            }

            if (next.frame_size > audio_end - next_offset) {
                continue;
            }
            third_offset = next_offset + next.frame_size;
            *frame_offset = candidate_offset;
            *first_frame = candidate;
            *is_cbr = ((third_offset < audio_end)
                       && (MP3_ReadFrameAt(file, third_offset, &third) != 0U)
                       && (third.version_id == candidate.version_id)
                       && (third.sample_rate_hz == candidate.sample_rate_hz)
                       && (next.bitrate_bps == candidate.bitrate_bps)
                       && (third.bitrate_bps == candidate.bitrate_bps))
                ? 1U
                : 0U;
            return 1U;
        }

        if (bytes_read <= MP3_FRAME_HEADER_SIZE - 1U) {
            break;
        }
        offset += bytes_read - (MP3_FRAME_HEADER_SIZE - 1U);
    }

    return 0U;
}

static uint32_t MP3_FindAudioEnd(FIL *file, uint32_t file_size)
{
    uint8_t signature[3];

    if ((file_size >= MP3_ID3V1_TAG_SIZE)
        && (MP3_ReadAt(file, file_size - MP3_ID3V1_TAG_SIZE,
                       signature, sizeof(signature)) != 0U)
        && (signature[0] == 'T') && (signature[1] == 'A')
        && (signature[2] == 'G')) {
        return file_size - MP3_ID3V1_TAG_SIZE;
    }
    return file_size;
}

void MP3_MetadataRead(FIL *file,
                      uint32_t file_size,
                      uint8_t *scratch,
                      uint16_t scratch_size,
                      mp3_metadata_t *metadata)
{
    mp3_frame_info_t first_frame;
    uint32_t id3_end = 0U;
    uint32_t id3_duration = 0U;
    uint32_t frame_offset = 0U;
    uint32_t audio_end;
    uint8_t is_cbr = 0U;

    if (metadata == NULL) {
        return;
    }

    memset(metadata, 0, sizeof(*metadata));
    if (file == NULL) {
        return;
    }

    (void)MP3_ReadId3v2(file, file_size, &id3_end, &id3_duration);
    audio_end = MP3_FindAudioEnd(file, file_size);
    if (MP3_FindFirstFrame(file, id3_end, audio_end,
                           scratch, scratch_size,
                           &frame_offset, &first_frame, &is_cbr) != 0U) {
        metadata->audio_offset = frame_offset;
        metadata->audio_size = audio_end - frame_offset;
        if (id3_duration != 0U) {
            metadata->duration_seconds = id3_duration;
        } else if ((is_cbr != 0U) && (first_frame.bitrate_bps != 0U)) {
            metadata->duration_seconds = (uint32_t)(
                ((uint64_t)metadata->audio_size * 8U)
                / first_frame.bitrate_bps);
        }
        return;
    }

    metadata->audio_offset = id3_end;
    metadata->audio_size = (audio_end > id3_end)
        ? audio_end - id3_end
        : 0U;
    metadata->duration_seconds = id3_duration;
}

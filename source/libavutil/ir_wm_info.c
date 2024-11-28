/*
* Copyright (c) 2022 Irdeto B.V.
*
* This file is part of FFmpeg.
*
* FFmpeg is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* FFmpeg is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <pthread.h>
#include <stdlib.h>
#include "error.h"
#include "mem.h"
#include "ir_wm_info.h"

static pthread_mutex_t    shared_data_locker = PTHREAD_MUTEX_INITIALIZER;
static volatile long long shared_epoch_time  = -1LL;

static uint8_t uuid_irdeto[] =
{
    0x40, 0x62, 0xB6, 0x99, 0x58, 0xC7, 0x44, 0x20, 0x81, 0xFA, 0x09, 0x10, 0x40, 0x88, 0x20, 0x77
};

static uint8_t uuid_dash_if[] =
{
    0xBE, 0xC4, 0xF8, 0x24, 0x17, 0x0D, 0x47, 0xCF, 0xA8, 0x26, 0xCE, 0x00, 0x80, 0x83, 0xE3, 0x55
};

static int av_wm_info_alloc_sei_irdeto_v2(AVIrdetoWatermark* wm_info, void* sei_payload, size_t* payload_size)
{
    uint8_t* payload = (uint8_t*) sei_payload;
    size_t   size    = sizeof(uuid_irdeto) + 6; /* 22 bytes */

    if (*payload_size < size)
        return AVERROR(EINVAL);

    /* 16 bytes: uuid */
    memcpy(payload, uuid_irdeto, sizeof(uuid_irdeto));

    /* 1 byte: version (2) */
    payload[16] = 0x02;

    /* 1 byte: encryption_flag (0), sync_info_present_flag (0), reserved_zero_bits */
    payload[17] = 0x00;

    /* 4 bytes: bitpos, bitval */
    payload[18]  = (wm_info->bitpos >> 23) & 0xFF;
    payload[19]  = (wm_info->bitpos >> 15) & 0xFF;
    payload[20]  = (wm_info->bitpos >>  7) & 0xFF;
    payload[21]  = (wm_info->bitpos <<  1) & 0xFE;
    payload[21] |=  wm_info->bitval        & 0x01;

    *payload_size = size;
    return 0;
}

static int av_wm_info_alloc_sei_irdeto_v3(AVIrdetoWatermark* wm_info, void* sei_payload, size_t* payload_size)
{
    uint8_t* payload = (uint8_t*) sei_payload;
    size_t   size    = sizeof(uuid_irdeto) + 11; /* 27 bytes */

    if ((*payload_size < size) || (wm_info->bitlen < 128) || (wm_info->bitlen > 8192) || ((wm_info->bitlen & 0x7F) != 0))
        return AVERROR(EINVAL);

    /* 16 bytes: uuid */
    memcpy(payload, uuid_irdeto, sizeof(uuid_irdeto));

    /* 1 byte: version (3) */
    payload[16] = 0x03;

    /* 1 byte: tmid_length_div128 (bitlen / 128), variant (bitval) */
    payload[17]  = (wm_info->bitlen / 128) << 1;
    payload[17] |=  wm_info->bitval & 1;

    /* 2 bytes: emulation_1 (1), sequence_id_0_14 (bitpos & 0x7FFF) */
    payload[18]  = 0x80;
    payload[18] |= (wm_info->bitpos >> 8) & 0x7F;
    payload[19]  =  wm_info->bitpos       & 0xFF;

    /* 1 byte: emulation_2 (1), firstpart (1), lastpart (1), reserved (0), sequence_id_15_18 ((bitpos >> 15) & 0x0F) */
    payload[20]  = 0xE0;
    payload[20] |= (wm_info->bitpos >> 15) & 0x0F;

    /* 2 bytes: emulation_3 (1), sequence_id_19_33 ((bitpos >> 19) & 0x7FFF) */
    payload[21]  = 0x80;
    payload[21] |= (wm_info->bitpos >> 27) & 0x7F;
    payload[22]  = (wm_info->bitpos >> 19) & 0xFF;

    /* 2 bytes: emulation_4 (1), sequence_id_34_48 ((bitpos >> 34) & 0x7FFF) */
    payload[23]  = 0x80;
    payload[23] |= (wm_info->bitpos >> 42) & 0x7F;
    payload[24]  = (wm_info->bitpos >> 34) & 0xFF;

    /* 2 bytes: emulation_5 (1), sequence_id_49_63 ((bitpos >> 49) & 0x7FFF) */
    payload[25]  = 0x80;
    payload[25] |= (wm_info->bitpos >> 57) & 0x7F;
    payload[26]  = (wm_info->bitpos >> 49) & 0xFF;

    *payload_size = size;
    return 0;
}

static int av_wm_info_alloc_sei_dash_if(AVIrdetoWatermark* wm_info, void* sei_payload, size_t* payload_size)
{
    /* In the case of irdeto-ffmpeg both encoders (AVC and HEVC) generate new GOP (new VOD segment)
       and WM-info SEI only when bitposition is changed, so firstpart and lastpart are always equal to 1 */

    uint8_t* payload = (uint8_t*) sei_payload;
    size_t   size    = sizeof(uuid_dash_if) + 5; /* 21 bytes */

    if (*payload_size < size)
        return AVERROR(EINVAL);

    /* 16 bytes: uuid */
    memcpy(payload, uuid_dash_if, sizeof(uuid_dash_if));

    /* 1 byte: version (1) */
    payload[16] = 0x01;

    /* 1 byte: variant (bitval) */
    payload[17] = wm_info->bitval & 0xFF;

    /* 2 bytes: emulation_1 (1), position (bitpos) */
    payload[18]  = 0x80;
    payload[18] |= (wm_info->bitpos >> 8) & 0x7F;
    payload[19]  =  wm_info->bitpos       & 0xFF;

    /* 1 byte: emulation_2 (1), firstpart (1), lastpart (1), reserved (0) */
    payload[20] = 0xE0;

    *payload_size = size;
    return 0;
}
/*
// Previous version of DASH-IF SEI (up to the end of December 2022)
static int av_wm_info_alloc_sei_dash_if(AVIrdetoWatermark* wm_info, void* sei_payload, size_t* payload_size)
{
    uint8_t* payload = (uint8_t*) sei_payload;
    size_t   size    = sizeof(uuid_dash_if) + 6; // 22 bytes

    if (*payload_size < size)
        return AVERROR(EINVAL);

    // 16 bytes: uuid
    memcpy(payload, uuid_dash_if, sizeof(uuid_dash_if));

    // 1 byte: version (1)
    payload[16] = 0x01;

    // 1 byte: variant (bitval)
    payload[17] = wm_info->bitval & 0xFF;

    // 2 bytes: emulation_1 (1), pos (bitpos)
    payload[18]  = 0x80;
    payload[18] |= (wm_info->bitpos >> 8) & 0x7F;
    payload[19]  =  wm_info->bitpos       & 0xFF;

    // 1 byte: firstpart (1), emulation_2 (1), iswm (1), reserved (0)
    payload[20] = 0xE0;

    // 1 byte: nbpart (1)
    payload[21] = 0x01;

    *payload_size = size;
    return 0;
}
*/
int av_wm_info_alloc_sei(AVIrdetoWatermark* wm_info, AVIrdetoSeiType type, void* sei_payload, size_t* payload_size)
{
    if ((NULL == wm_info) || (NULL == sei_payload) || (NULL == payload_size))
        return AVERROR(EINVAL);

    switch (type)
    {
        case AV_WM_SEI_IRDETO_V2:
            return av_wm_info_alloc_sei_irdeto_v2(wm_info, sei_payload, payload_size);

        case AV_WM_SEI_IRDETO_V3:
            return av_wm_info_alloc_sei_irdeto_v3(wm_info, sei_payload, payload_size);

        case AV_WM_SEI_DASH_IF:
            return av_wm_info_alloc_sei_dash_if(wm_info, sei_payload, payload_size);

        case AV_WM_SEI_NONE:
        default:
            break;
    }

    return AVERROR(EINVAL);
}

int av_wm_info_atoi(const char* in, unsigned int* out)
{
    char* end = NULL;
    unsigned int val = (in != NULL) ? (unsigned int) strtol(in, &end, 0) : 0;
 
    if (end == NULL || end == in || *end != '\0')
        return AVERROR(EINVAL);

    if (out != NULL)
        *out = val;

    return 0;
}

int av_wm_info_atoll(const char* in, unsigned long long* out)
{
    char* end = NULL;
    unsigned long long val = (in != NULL) ? (unsigned long long) strtoll(in, &end, 0) : 0;
 
    if (end == NULL || end == in || *end != '\0')
        return AVERROR(EINVAL);

    if (out != NULL)
        *out = val;

    return 0;
}

int av_wm_info_reset_epoch_time(void)
{
    pthread_mutex_lock(&shared_data_locker);
    shared_epoch_time = -1LL;
    pthread_mutex_unlock(&shared_data_locker);

    return 0;
}

int av_wm_info_set_epoch_time(long long epoch_time)
{
    int result = 0;

    pthread_mutex_lock(&shared_data_locker);
    if (shared_epoch_time < 0LL)
        shared_epoch_time = epoch_time;
    else
        result = AVERROR(EUCLEAN);
    pthread_mutex_unlock(&shared_data_locker);

    return result;
}

int av_wm_info_get_epoch_time(long long* p_epoch_time)
{
    if (! p_epoch_time)
        return AVERROR(EINVAL);

    pthread_mutex_lock(&shared_data_locker);
    *p_epoch_time = shared_epoch_time;
    pthread_mutex_unlock(&shared_data_locker);

    return 0;
}

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

#ifndef _IR_WM_INFO_H_
#define _IR_WM_INFO_H_

#include <stdint.h>

/**
********************************************************************************
* @enum   AVIrdetoSeiType
* @brief  It specifies the format of SEI payload
********************************************************************************
*/
typedef enum AVIrdetoSeiType
{
    AV_WM_SEI_NONE = 0,
    AV_WM_SEI_IRDETO_V2,
    AV_WM_SEI_IRDETO_V3,
    AV_WM_SEI_DASH_IF

} AVIrdetoSeiType;

/**
********************************************************************************
* @struct AVIrdetoWatermark
* @brief  Irdeto watermark info structure
********************************************************************************
*/
typedef struct AVIrdetoWatermark
{
    unsigned int       bitlen;      ///< TMID length (in bits)
    unsigned int       bitval;      ///< TMID bit value
    unsigned long long bitpos;      ///< TMID bit position
    unsigned int       watermarked; ///< Frame is watermarked

} AVIrdetoWatermark;

/**
********************************************************************************
* @brief  Routine to generate H264/H265 SEI with Irdeto watermark info inside
* @note   Returns 0 on success, negative value on failure
********************************************************************************
*/
int av_wm_info_alloc_sei(AVIrdetoWatermark* wm_info, AVIrdetoSeiType type, void* sei_payload, size_t* payload_size);

/**
********************************************************************************
* @brief  Simple wrapper for routine of conversion from text to int
* @note   Returns 0 on success, negative value on failure
********************************************************************************
*/
int av_wm_info_atoi(const char* in, unsigned int* out);

/**
********************************************************************************
* @brief  Simple wrapper for routine of conversion from text to long long
* @note   Returns 0 on success, negative value on failure
********************************************************************************
*/
int av_wm_info_atoll(const char* in, unsigned long long* out);

/**
********************************************************************************
* @brief  A group of calls to share Initial Epoch Time value between components
* @note   Returns 0 on success, negative value on failure
********************************************************************************
*/
int av_wm_info_reset_epoch_time(void);
int av_wm_info_set_epoch_time(long long epoch_time);
int av_wm_info_get_epoch_time(long long* p_epoch_time);

#endif /* !_IR_WM_INFO_H_ */

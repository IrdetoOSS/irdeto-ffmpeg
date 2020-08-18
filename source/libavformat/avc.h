/*
 * AVC helper functions for muxers
 * Copyright (c) 2008 Aurelien Jacobs <aurel@gnuage.org>
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

#ifndef AVFORMAT_AVC_H
#define AVFORMAT_AVC_H

#include <stdint.h>
#include "avio.h"

/**
* @typedef avc_extra_data_parse_ctx_t
* @brief   handler context of the AVC extra data (actually sps and pps)
*          parsing for cenc/cbcs v2/v3 encryption. The avc extra data
*          is from AVCodecParameters->extradata
* @note     call ff_avc_mp4_parse_extradata_init first to init parsing handler
*           call ff_avc_mp4_parse_extradata first for parsing and then
*           call ff_avc_parser_get_slice_header_size of each vcl nal (determined by ff_avc_parser_is_vcl)
*                to get slice header + nal header size, finally
*           call ff_avc_mp4_parse_extradata_clean for resource cleaning
*           This series of function is only for one stream. For different stream, a new handler is needed
*/
typedef void *avc_extra_data_parse_ctx_t;

/**
* @brief    Check the given nal type is vcl
* @param    [in] type    avc nal type
* @note     This is a helper function for CENC cenc and cbcs encryption, 1 <= type <=5 is vcl
* @return   1 if is vcl
*           0 otherwise
*/
int ff_avc_parser_is_vcl(const uint8_t type);

/**
* @brief    Init avc codec extra data parsing handler
* @param    [in out] ctx    pointer to parsing handler context
* @return   0 if success
*           -1 otherwise
*/
void ff_avc_mp4_parse_extradata_init(avc_extra_data_parse_ctx_t * ctx);

/**
* @brief    Parse AVC codec extra data
* @param    [in out] ctx    parsing handler context
* @param    [in] buf        pointer to codec extra data, AVCodecParameters->extradata
* @param    [in] size       size of buf
* @return   0 if success
*           -1 otherwise
*/
int ff_avc_mp4_parse_extradata(avc_extra_data_parse_ctx_t ctx,
                               const uint8_t * buf, int size);

/**
* @brief    Get the nal header + slice header of given vcl nal
* @note     The func will not check the input nal is vcl
* @param    [in] ctx        parsing handler context
* @param    [in] buf        buf
* @param    [in] buf_size   size of buf
* @param    [out] size      size of nal header + slice header in bytes
* @ref      9.5.2.2 Subsample encryption applied to NAL Structure,
*           ISO/IEC 23001-7 (2016) Information technology — MPEG systems technologies —
*           Part 7: Common encryption in ISO base media file format files
* @return   0 if success
*           -1 otherwise
*/
int ff_avc_parser_get_slice_header_size(avc_extra_data_parse_ctx_t ctx,
                                        const uint8_t * const buf,
                                        size_t buf_size, size_t * size);
/**
* @brief    Clearn parser context
* @param    [in] ctx        parsing handler context
*/
void ff_avc_mp4_parse_extradata_clean(avc_extra_data_parse_ctx_t ctx);

int ff_avc_parse_nal_units(AVIOContext * s, const uint8_t * buf, int size);
int ff_avc_parse_nal_units_buf(const uint8_t * buf_in, uint8_t ** buf,
                               int *size);
int ff_isom_write_avcc(AVIOContext * pb, const uint8_t * data, int len);
const uint8_t *ff_avc_find_startcode(const uint8_t * p,
                                     const uint8_t * end);
int ff_avc_write_annexb_extradata(const uint8_t * in, uint8_t ** buf,
                                  int *size);
const uint8_t *ff_avc_mp4_find_startcode(const uint8_t * start,
                                         const uint8_t * end,
                                         int nal_length_size);

#endif                          /* AVFORMAT_AVC_H */

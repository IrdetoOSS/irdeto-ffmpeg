/*
 * MOV CENC (Common Encryption) cenc scheme v3
 * Copyright (c) 2015 Eran Kornblau <erankor at gmail dot com>
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

#ifndef AVFORMAT_MOVENCCENCV3_H
#define AVFORMAT_MOVENCCENCV3_H

#include "libavutil/aes_ctr.h"
#include "avformat.h"
#include "avio.h"
#include "movenccenc.h"
/**
 * @brief Initialize a CENC context
 * @param ctx             pointer to parsing handler context
 * @param encryption_key  encryption key, must have a length of AES_CTR_KEY_SIZE
 * @param use_subsamples  subsample flag, 1 if switch on, 0 if off. Subsample
 *                        encryption only support codec avc currently
 * @param bitexact        random generated iv or use default 0 iv
 * @note     call ff_mov_cencv3_init first to init
 *           call ff_mov_cencv3_parse_avc_XPS if the codec is AVC to parse
 *           call the other function related to sample parsing and writing
 *           call ff_mov_cencv3_free for resource cleaning
 */
int ff_mov_cencv3_init(MOVMuxCencContext * ctx, uint8_t * encryption_key,
                       int use_subsamples, int bitexact);

/**
* @brief    Parse AVC codec extra data
* @param    [in out] ctx    parsing handler context
* @param    [in] extra_data pointer to codec extra data, AVCodecParameters->extradata
* @param    [in] size       size of buf
* @return   0 if success
*           -1 otherwise
*/
int ff_mov_cencv3_parse_avc_XPS(MOVMuxCencContext * ctx,
                                uint8_t * extra_data, int size);

/**
 * Free a CENC context
 */
void ff_mov_cencv3_free(MOVMuxCencContext * ctx);

/**
 * Write a fully encrypted packet
 */
int ff_mov_cencv3_write_packet(MOVMuxCencContext * ctx, AVIOContext * pb,
                               const uint8_t * buf_in, int size);

/**
 * Parse AVC NAL units from annex B format, the nal size and type are written in the clear while the body is encrypted
 */
int ff_mov_cencv3_avc_parse_nal_units(MOVMuxCencContext * ctx,
                                      AVIOContext * pb,
                                      const uint8_t * buf_in, int size);

/**
 * Write AVC NAL units that are in MP4 format, the nal size and type are written in the clear while the body is encrypted
 */
int ff_mov_cencv3_avc_write_nal_units(AVFormatContext * s,
                                      MOVMuxCencContext * ctx,
                                      int nal_length_size,
                                      AVIOContext * pb,
                                      const uint8_t * buf_in, int size);

/**
 * Write the cenc atoms that should reside inside stbl
 */
void ff_mov_cencv3_write_stbl_atoms(MOVMuxCencContext * ctx,
                                    AVIOContext * pb);

/**
 * Write the sinf atom, contained inside stsd
 */
int ff_mov_cencv3_write_sinf_tag(struct MOVTrack *track, AVIOContext * pb,
                                 uint8_t * kid);

#endif                          /* AVFORMAT_MOVENCCENCV3_H */

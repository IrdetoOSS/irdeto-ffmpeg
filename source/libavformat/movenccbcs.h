/*
 * MOV CENC (Common Encryption) writer cbcs scheme
 * Copyright (c) 2019 Chunqiu Lu <chunqiu.lu@irdeto.com>
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

#ifndef AVFORMAT_MOVENCCBCS_H
#define AVFORMAT_MOVENCCBCS_H

#include "libavutil/aes.h"
#include "avformat.h"
#include "avio.h"
#include "avc.h"

#define CBCS_KID_SIZE (16)
#define CBCS_KEY_SIZE (16)
#define CBCS_IV_SIZE (16)
#define CBCS_KEY_SIZE_BITS (CBCS_KEY_SIZE * 8)
#define CBCS_PATTERN_DEFAULT_CRYPT_BLOCK_NUM (1)
#define CBCS_PATTERN_DEFAULT_SKIP_BLOCK_NUM (9)
#define CBCS_PATTERN_BLOCK_NUM (10)

struct MOVTrack;

typedef struct {
    struct AVAES *aes_cbc;
    uint8_t *auxiliary_info;
    size_t auxiliary_info_size;
    size_t auxiliary_info_alloc_size;
    uint32_t auxiliary_info_entries;
    uint8_t *iv;
    int crypt_byte_block;       // for pattern encryption cbcs
    int skip_byte_block;        // for pattern encryption cbcs

    /* subsample support */
    int use_subsamples;
    uint16_t subsample_count;
    size_t auxiliary_info_subsample_start;
    uint8_t *auxiliary_info_sizes;
    size_t auxiliary_info_sizes_alloc_size;
    avc_extra_data_parse_ctx_t avc_extra_data_parse_ctx;
} MOVMuxCbcsContext;


/**
 * @brief Initialize a CENC context
 * @param ctx             pointer to parsing handler context
 * @param encryption_key  encryption key, must have a length of 16
 * @param encryption_iv   encryption iv, must have a length of 16, set by ff_mov_cbcs_set_random_iv
 * @param crpt_block      num of cryption block in cbcs pattern
 * @param skip_block      num of skipped block in cbcs pattern, crpt_block + skip_block = 10
 * @param use_subsamples  subsample flag, 1 if switch on, 0 if off. Subsample
 *                        encryption only support codec avc currently
 * @note     call ff_mov_cbcs_init first to init
 *           call ff_mov_avc_cbcs_parse_XPS if the codec is AVC to parse
 *           call the other function related to sample parsing and writing
 *           call ff_mov_cbcs_free for resource cleaning
 */
int ff_mov_cbcs_init(MOVMuxCbcsContext * ctx,
                     const uint8_t * const encryption_key,
                     const uint8_t * const encryption_iv, int crpt_block,
                     int skip_block, int use_subsamples);

/**
* @brief    set random 16 bytes iv
* @param    [in out] iv     predefined 16 byte iv buffer
* @param    [in] bitexact   0 if random, 1 otherwise use the default value
*/
void ff_mov_cbcs_set_random_iv(uint8_t * iv, int bitexact);

/**
* @brief    Parse AVC codec extra data
* @param    [in out] ctx    parsing handler context
* @param    [in] extra_data pointer to codec extra data, AVCodecParameters->extradata
* @param    [in] size       size of buf
* @return   0 if success
*           -1 otherwise
*/
int ff_mov_avc_cbcs_parse_XPS(MOVMuxCbcsContext * ctx,
                              uint8_t * extra_data, int size);

/**
 * Free a CENC context
 */
void ff_mov_cbcs_free(MOVMuxCbcsContext * ctx);

/**
 * Write a fully encrypted packet
 */
int ff_mov_cbcs_write_packet(MOVMuxCbcsContext * ctx, AVIOContext * pb,
                             const uint8_t * buf_in, int size);

/**
 * Parse AVC NAL units from annex B format, the nal size and type are written in the clear while the body is encrypted
 */
int ff_mov_cbcs_avc_parse_nal_units(MOVMuxCbcsContext * ctx,
                                    AVIOContext * pb,
                                    const uint8_t * buf_in, int size);

/**
 * Write AVC NAL units that are in MP4 format, the nal size and type are written in the clear while the body is encrypted
 */
int ff_mov_cbcs_avc_write_nal_units(AVFormatContext * s,
                                    MOVMuxCbcsContext * ctx,
                                    int nal_length_size, AVIOContext * pb,
                                    const uint8_t * buf_in, int size);

/**
 * Write the cbcs atoms that should reside inside stbl
 */
void ff_mov_cbcs_write_stbl_atoms(MOVMuxCbcsContext * ctx,
                                  AVIOContext * pb);

/**
 * Write the sinf atom, contained inside stsd
 */
int ff_mov_cbcs_write_sinf_tag(struct MOVTrack *track, AVIOContext * pb,
                               uint8_t * kid);

#endif                          /* AVFORMAT_MOVENCCBCS_H */

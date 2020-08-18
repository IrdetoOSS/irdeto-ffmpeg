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
#include "movenccbcs.h"
#include "libavutil/intreadwrite.h"
#include "avio_internal.h"
#include "movenc.h"
#include "libavutil/random_seed.h"
static int auxiliary_info_alloc_size(MOVMuxCbcsContext * ctx, int size)
{
    size_t new_alloc_size;

    if (ctx->auxiliary_info_size + size > ctx->auxiliary_info_alloc_size) {
        new_alloc_size =
            FFMAX(ctx->auxiliary_info_size + size,
                  ctx->auxiliary_info_alloc_size * 2);
        if (av_reallocp(&ctx->auxiliary_info, new_alloc_size)) {
            return AVERROR(ENOMEM);
        }

        ctx->auxiliary_info_alloc_size = new_alloc_size;
    }

    return 0;
}

static int auxiliary_info_write(MOVMuxCbcsContext * ctx,
                                const uint8_t * buf_in, int size)
{
    int ret;

    ret = auxiliary_info_alloc_size(ctx, size);
    if (ret) {
        return ret;
    }
    memcpy(ctx->auxiliary_info + ctx->auxiliary_info_size, buf_in, size);
    ctx->auxiliary_info_size += size;

    return 0;
}

static int auxiliary_info_add_subsample(MOVMuxCbcsContext * ctx,
                                        uint16_t clear_bytes,
                                        uint32_t encrypted_bytes)
{
    uint8_t *p;
    int ret;

    if (!ctx->use_subsamples) {
        return 0;
    }

    ret = auxiliary_info_alloc_size(ctx, 6);
    if (ret) {
        return ret;
    }

    p = ctx->auxiliary_info + ctx->auxiliary_info_size;

    AV_WB16(p, clear_bytes);
    p += sizeof(uint16_t);

    AV_WB32(p, encrypted_bytes);

    ctx->auxiliary_info_size += 6;
    ctx->subsample_count++;

    return 0;
}

/**
 * Encrypt the input buffer and write using avio_write
 */
static void mov_cbcs_write_encrypted(MOVMuxCbcsContext * ctx,
                                     AVIOContext * pb,
                                     const uint8_t * buf_in,
                                     const int size)
{
    uint8_t chunk[256];         //< 256 is definitely > whole pattern size (16 * 10)
    const size_t AES128_BLOCK_SIZE = 16;
    size_t processed_size = 0;
    int res_size = 0;
    const size_t pattern_crypted_size =
        ctx->crypt_byte_block * AES128_BLOCK_SIZE;
    const size_t pattern_skipped_size =
        ctx->skip_byte_block * AES128_BLOCK_SIZE;
    uint8_t iv_intermediate[CBCS_IV_SIZE];
    memcpy(iv_intermediate, ctx->iv, CBCS_IV_SIZE);     //restart iv from each sample
    while (processed_size < size) {
        int bytes_remaining = size - processed_size;

        // encrypt and write encrypted block
        if (bytes_remaining > pattern_crypted_size) {
            av_aes_crypt(ctx->aes_cbc, chunk, buf_in + processed_size,
                         ctx->crypt_byte_block, iv_intermediate, 0);
            avio_write(pb, chunk, pattern_crypted_size);
            processed_size += pattern_crypted_size;
            bytes_remaining = size - processed_size;
        }
        //calculate remaining bytes again to get clear size
        // Bypass next clear block
        size_t clear_size =
            (pattern_skipped_size >
             bytes_remaining) ? bytes_remaining : pattern_skipped_size;
        avio_write(pb, buf_in + processed_size, clear_size);
        processed_size += clear_size;
    }

}

/**
 * Start writing a packet
 */
static int mov_cbcs_start_packet(MOVMuxCbcsContext * ctx)
{
    int ret;

    if (!ctx->use_subsamples) {
        return 0;
    }

    /* write a zero subsample count */
    ctx->auxiliary_info_subsample_start = ctx->auxiliary_info_size;
    ctx->subsample_count = 0;
    ret =
        auxiliary_info_write(ctx, (uint8_t *) & ctx->subsample_count,
                             sizeof(ctx->subsample_count));
    if (ret) {
        return ret;
    }

    return 0;
}

/**
 * Finalize a packet
 */
static int mov_cbcs_end_packet(MOVMuxCbcsContext * ctx)
{
    size_t new_alloc_size;
    if (!ctx->use_subsamples) {
        ctx->auxiliary_info_entries++;
        return 0;
    }

    /* add the auxiliary info entry size */
    if (ctx->auxiliary_info_entries >=
        ctx->auxiliary_info_sizes_alloc_size) {
        new_alloc_size = ctx->auxiliary_info_entries * 2 + 1;
        if (av_reallocp(&ctx->auxiliary_info_sizes, new_alloc_size)) {
            return AVERROR(ENOMEM);
        }

        ctx->auxiliary_info_sizes_alloc_size = new_alloc_size;
    }
    ctx->auxiliary_info_sizes[ctx->auxiliary_info_entries] =
        AES_CTR_IV_SIZE + ctx->auxiliary_info_size -
        ctx->auxiliary_info_subsample_start;
    ctx->auxiliary_info_entries++;

    /* update the subsample count */
    AV_WB16(ctx->auxiliary_info + ctx->auxiliary_info_subsample_start,
            ctx->subsample_count);

    return 0;
}

int ff_mov_cbcs_write_packet(MOVMuxCbcsContext * ctx, AVIOContext * pb,
                             const uint8_t * buf_in, int size)
{
    int ret;

    ret = mov_cbcs_start_packet(ctx);
    if (ret) {
        return ret;
    }

    ret = auxiliary_info_add_subsample(ctx, 0, size);
    if (ret) {
        return ret;
    }

    mov_cbcs_write_encrypted(ctx, pb, buf_in, size);

    ret = mov_cbcs_end_packet(ctx);
    if (ret) {
        return ret;
    }

    return 0;
}

int ff_mov_cbcs_avc_parse_nal_units(MOVMuxCbcsContext * ctx,
                                    AVIOContext * pb,
                                    const uint8_t * buf_in, int size)
{
    const uint8_t *p = buf_in;
    const uint8_t *end = p + size;
    const uint8_t *nal_start, *nal_end;
    int ret = mov_cbcs_start_packet(ctx);
    if (ret) {
        return ret;
    }

    int clear_bytes = 0;
    int encrypted_bytes = 0;
    nal_start = ff_avc_find_startcode(p, end);
    size = 0;
    while (1) {

        while (nal_start < end && !*(nal_start++));
        if (nal_start == end)
            break;
        nal_end = ff_avc_find_startcode(nal_start, end);
        int nalsize = nal_end - nal_start;

        avio_wb32(pb, nalsize);
        clear_bytes += 4;

        int vcl = ff_avc_parser_is_vcl(*nal_start & 0x1f);
        //non-vcl, write whole clear nal
        if (0 == vcl) {
            avio_write(pb, nal_start, nalsize);
            clear_bytes += nalsize;
        } else                  //vcl, write nal header and slice header in clear and encrypt rest
        {
            size_t slice_header_size = 0;       // nal header + slice header
            int res =
                ff_avc_parser_get_slice_header_size
                (ctx->avc_extra_data_parse_ctx,
                 nal_start,
                 nalsize,
                 &slice_header_size);
            if (0 != res) {
                return -1;
            }
            avio_write(pb, nal_start, slice_header_size);
            clear_bytes += slice_header_size;

            mov_cbcs_write_encrypted(ctx, pb,
                                     nal_start + slice_header_size,
                                     nalsize - slice_header_size);
            encrypted_bytes += (nalsize - slice_header_size);
            //nal_start += (nalsize - slice_header_size);
            auxiliary_info_add_subsample(ctx, clear_bytes,
                                         encrypted_bytes);
            clear_bytes = 0;
            encrypted_bytes = 0;
        }
        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }

    //if the sample is all non-vcl which is most likely impossible, add subsample with all clear data
    if (0 != clear_bytes && 0 == encrypted_bytes) {
        auxiliary_info_add_subsample(ctx, clear_bytes, encrypted_bytes);
    }

    ret = mov_cbcs_end_packet(ctx);
    if (ret) {
        return ret;
    }

    return size;
}

int ff_mov_cbcs_avc_write_nal_units(AVFormatContext * s,
                                    MOVMuxCbcsContext * ctx,
                                    int nal_length_size, AVIOContext * pb,
                                    const uint8_t * buf_in, int size)
{
    int nalsize;
    int ret;
    int j;

    ret = mov_cbcs_start_packet(ctx);
    if (ret) {
        return ret;
    }
    int clear_bytes = 0;
    int encrypted_bytes = 0;
    while (size > 0) {

        /* parse the nal size */
        if (size < nal_length_size + 1) {
            av_log(s, AV_LOG_ERROR,
                   "CBCS-AVC: remaining size %d smaller than nal length+type %d\n",
                   size, nal_length_size + 1);
            return -1;
        }
        avio_write(pb, buf_in, nal_length_size);
        size -= nal_length_size;
        clear_bytes += nal_length_size;

        nalsize = 0;
        for (j = 0; j < nal_length_size; j++) {
            nalsize = (nalsize << 8) | *buf_in++;
        }

        if (nalsize <= 0 || nalsize > size) {
            av_log(s, AV_LOG_ERROR, "CBCS-AVC: nal size %d remaining %d\n",
                   nalsize, size);
            return -1;
        }

        int vcl = ff_avc_parser_is_vcl(*buf_in & 0x1f);
        //not vcl, write whole clear nal
        if (0 == vcl) {
            avio_write(pb, buf_in, nalsize);
            size -= nalsize;
            clear_bytes += nalsize;
            buf_in += nalsize;
        } else                  //vcl, write nal header and slice header in clear and encrypt rest
        {
            size_t slice_header_size = 0;       // nal header + slice header
            int res =
                ff_avc_parser_get_slice_header_size
                (ctx->avc_extra_data_parse_ctx,
                 buf_in,
                 nalsize,
                 &slice_header_size);
            if (0 != res) {
                av_log(s, AV_LOG_ERROR,
                       "CENC-AVC: failed to get slice header size\n");
                return -1;
            }
            avio_write(pb, buf_in, slice_header_size);
            clear_bytes += slice_header_size;
            buf_in += slice_header_size;

            mov_cbcs_write_encrypted(ctx, pb, buf_in,
                                     nalsize - slice_header_size);
            encrypted_bytes += (nalsize - slice_header_size);
            buf_in += (nalsize - slice_header_size);
            size -= nalsize;
            auxiliary_info_add_subsample(ctx, clear_bytes,
                                         encrypted_bytes);
            clear_bytes = 0;
            encrypted_bytes = 0;
        }
    }

    //if the sample is all non-vcl which is most likely impossible, add subsample with all clear data
    if (0 != clear_bytes && 0 == encrypted_bytes) {
        auxiliary_info_add_subsample(ctx, clear_bytes, encrypted_bytes);
    }


    ret = mov_cbcs_end_packet(ctx);
    if (ret) {
        return ret;
    }

    return 0;
}

/* TODO: reuse this function from movenc.c */
static int64_t update_size(AVIOContext * pb, int64_t pos)
{
    int64_t curpos = avio_tell(pb);
    avio_seek(pb, pos, SEEK_SET);
    avio_wb32(pb, curpos - pos);        /* rewrite size */
    avio_seek(pb, curpos, SEEK_SET);

    return curpos - pos;
}

static int mov_cbcs_write_senc_tag(MOVMuxCbcsContext * ctx,
                                   AVIOContext * pb,
                                   int64_t * auxiliary_info_offset)
{
    int64_t pos = avio_tell(pb);

    avio_wb32(pb, 0);           /* size */
    ffio_wfourcc(pb, "senc");
    avio_wb32(pb, ctx->use_subsamples ? 0x02 : 0);      /* version & flags */
    avio_wb32(pb, ctx->auxiliary_info_entries); /* entry count */
    *auxiliary_info_offset = avio_tell(pb);
    avio_write(pb, ctx->auxiliary_info, ctx->auxiliary_info_size);
    return update_size(pb, pos);
}

static int mov_cbcs_write_saio_tag(AVIOContext * pb,
                                   int64_t auxiliary_info_offset)
{
    int64_t pos = avio_tell(pb);
    uint8_t version;

    avio_wb32(pb, 0);           /* size */
    ffio_wfourcc(pb, "saio");
    version = auxiliary_info_offset > 0xffffffff ? 1 : 0;
    avio_w8(pb, version);
    avio_wb24(pb, 0);           /* flags */
    avio_wb32(pb, 1);           /* entry count */
    if (version) {
        avio_wb64(pb, auxiliary_info_offset);
    } else {
        avio_wb32(pb, auxiliary_info_offset);
    }
    return update_size(pb, pos);
}

static int mov_cbcs_write_saiz_tag(MOVMuxCbcsContext * ctx,
                                   AVIOContext * pb)
{
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0);           /* size */
    ffio_wfourcc(pb, "saiz");
    avio_wb32(pb, 0);           /* version & flags */
    avio_w8(pb, ctx->use_subsamples ? 0 : AES_CTR_IV_SIZE);     /* default size */
    avio_wb32(pb, ctx->auxiliary_info_entries); /* entry count */
    if (ctx->use_subsamples) {
        avio_write(pb, ctx->auxiliary_info_sizes,
                   ctx->auxiliary_info_entries);
    }
    return update_size(pb, pos);
}

void ff_mov_cbcs_write_stbl_atoms(MOVMuxCbcsContext * ctx,
                                  AVIOContext * pb)
{
    int64_t auxiliary_info_offset;

    mov_cbcs_write_senc_tag(ctx, pb, &auxiliary_info_offset);
    mov_cbcs_write_saio_tag(pb, auxiliary_info_offset);
    mov_cbcs_write_saiz_tag(ctx, pb);
}

static int mov_cbcs_write_schi_tag(MOVMuxCbcsContext * ctx,
                                   AVIOContext * pb, uint8_t * kid)
{
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0);           /* size */
    ffio_wfourcc(pb, "schi");

    avio_wb32(pb, 49);          /* size */
    ffio_wfourcc(pb, "tenc");
    avio_w8(pb, 1);             // version
    avio_wb24(pb, 0);           //flags
    avio_w8(pb, 0);             // reserved
    avio_w8(pb, ctx->crypt_byte_block << 4 | ctx->skip_byte_block);     // crypted block + skip block
    avio_w8(pb, 1);             /* is encrypted */
    avio_w8(pb, 0);             /* iv size */
    avio_write(pb, kid, CBCS_KID_SIZE);
    avio_w8(pb, CBCS_IV_SIZE);  // iv size
    avio_write(pb, ctx->iv, CBCS_IV_SIZE);

    return update_size(pb, pos);
}

int ff_mov_cbcs_write_sinf_tag(MOVTrack * track, AVIOContext * pb,
                               uint8_t * kid)
{
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0);           /* size */
    ffio_wfourcc(pb, "sinf");

    /* frma */
    avio_wb32(pb, 12);          /* size */
    ffio_wfourcc(pb, "frma");
    avio_wl32(pb, track->tag);

    /* schm */
    avio_wb32(pb, 20);          /* size */
    ffio_wfourcc(pb, "schm");
    avio_wb32(pb, 0);           /* version & flags */
    ffio_wfourcc(pb, "cbcs");   /* scheme type */
    avio_wb32(pb, 0x10000);     /* scheme version */

    /* schi */
    mov_cbcs_write_schi_tag(&track->cbcs, pb, kid);

    return update_size(pb, pos);
}

int ff_mov_cbcs_init(MOVMuxCbcsContext * ctx,
                     const uint8_t * const encryption_key,
                     const uint8_t * const encryption_iv, int crypt_block,
                     int skip_block, int use_subsamples)
{
    int ret;

    ctx->aes_cbc = av_aes_alloc();
    if (!ctx->aes_cbc) {
        return AVERROR(ENOMEM);
    }
    if (crypt_block < 0 || skip_block < 0
        || (crypt_block + skip_block) != CBCS_PATTERN_BLOCK_NUM) {
        return AVERROR_INVALIDDATA;
    }
    ctx->crypt_byte_block = crypt_block;
    ctx->skip_byte_block = skip_block;
    ctx->iv = encryption_iv;
    ret = av_aes_init(ctx->aes_cbc, encryption_key, CBCS_KEY_SIZE_BITS, 0);
    if (ret != 0) {
        return ret;
    }

    ctx->use_subsamples = use_subsamples;
    return 0;
}


int ff_mov_avc_cbcs_parse_XPS(MOVMuxCbcsContext * ctx,
                              uint8_t * extra_data, int size)
{
    ff_avc_mp4_parse_extradata_init(&ctx->avc_extra_data_parse_ctx);
    return ff_avc_mp4_parse_extradata(ctx->avc_extra_data_parse_ctx,
                                      extra_data, size);
}

void ff_mov_cbcs_free(MOVMuxCbcsContext * ctx)
{
    av_freep(&ctx->aes_cbc);
    av_freep(&ctx->auxiliary_info);
    av_freep(&ctx->auxiliary_info_sizes);
    ff_avc_mp4_parse_extradata_clean(ctx->avc_extra_data_parse_ctx);
}

void ff_mov_cbcs_set_random_iv(uint8_t * iv, int bitexact)
{
    if (!bitexact) {
        const int block = CBCS_IV_SIZE >> 2;
        for (int i = 0; i < block; i++) {
            *(uint32_t *) (iv + 4 * i) = av_get_random_seed();
        }
    } else {
        memset(iv, 0, CBCS_IV_SIZE);
    }
}

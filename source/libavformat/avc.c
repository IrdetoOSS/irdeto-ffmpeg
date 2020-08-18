/*
 * AVC helper functions for muxers
 * Copyright (c) 2006 Baptiste Coudurier <baptiste.coudurier@smartjog.com>
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

#include "libavutil/intreadwrite.h"
#include "libavcodec/h264.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/golomb.h"
#include "avformat.h"
#include "avio.h"
#include "avc.h"

#define MAX_PPS_NUM 255         // 0 - 255
#define MAX_SPS_NUM 31          // 0 - 31

/**
* @brief    Structure of AVC sps info, prepared for the parsing of the vcl slice header, so it
*           only has partial sps info
*/
typedef struct {
    int exist_flag;
    int seq_parameter_set_id;
    uint8_t chroma_format_idc;
    uint8_t log2_max_frame_num;
    int pic_height_in_map_units;
    int pic_width_in_mbs;
    uint8_t pic_order_cnt_type;
    uint8_t delta_pic_order_always_zero_flag;
    uint8_t log2_max_pic_order_cnt_lsb;
    uint8_t chroma_array_type;
    uint8_t separate_colour_plane_flag;
    uint8_t frame_mbs_only_flag;
} avc_sps_t;

/**
* @brief    Structure of AVC pps info, prepared for the parsing of the vcl slice header, so it
*           only has partial pps info
*/
typedef struct {
    avc_sps_t *sps;
    int exist_flag;
    int pic_parameter_set_id;
    int seq_parameter_set_id;
    uint8_t entropy_coding_mode_flag;
    uint8_t bottom_field_pic_order_in_frame_present_flag;
    int slice_group_change_rate;
    int num_ref_idx[2];
    uint8_t slice_group_map_type;
    uint8_t num_slice_groups_minus1;
    uint8_t weighted_bipred_idc;
    uint8_t weighted_pred_flag;
    uint8_t deblocking_filter_control_present_flag;
    uint8_t redundant_pic_cnt_present_flag;
} avc_pps_t;

/**
* @brief    Internal AVC codec extra data parsing context. This is stupid wasting memory but simple.
*/
typedef struct {
    avc_pps_t pps_list[MAX_PPS_NUM];
    avc_sps_t sps_list[MAX_SPS_NUM];
} avc_extra_data_ctx_t;

/**
* @brief    Enum of AVC slie type prepared for slice header parsing.
*/
enum {
    AVC_SLICE_P = 0,
    AVC_SLICE_B = 1,
    AVC_SLICE_I = 2,
    AVC_SLICE_SP = 3,
    AVC_SLICE_SI = 4
};

/**
* @brief    Enum of AVC nal type
*/
enum {
    AVC_NAL_SLICE = 1,
    AVC_NAL_DPA = 2,
    AVC_NAL_DPB = 3,
    AVC_NAL_DPC = 4,
    AVC_NAL_IDR_SLICE = 5,
    AVC_NAL_SEI = 6,
    AVC_NAL_SPS = 7,
    AVC_NAL_PPS = 8
};

/**
* @brief    Parse AVC pps
* @note     The func assumes the input nal is pps and only parses partially to fill in all the info in avc_pps_t struct
* @param    [in] buf        pointer to pps buffer
* @param    [in] size       size of buf
* @param    [int out] pps   pointer to pre-allocated avc_pps_t struct
* @return   0 if success
*           -1 otherwise
*/
#ifdef IRDETO_UNIT_TEST
int avc_parser_parse_pps(const uint8_t * const buf, size_t size,
                         avc_pps_t * const pps);
#else
static int avc_parser_parse_pps(const uint8_t * const buf, size_t size,
                                avc_pps_t * const pps);
#endif

/**
* @brief    Parse AVC sps
* @note     The func assumes the input nal is sps and only parses partially to fill in all the info in avc_sps_t struct
* @param    [in] buf        pointer to sps buffer
* @param    [in] size       size of buf
* @param    [int out] sps   pointer to pre-allocated avc_pps_t struct
* @return   0 if success
*           -1 otherwise
*/
#ifdef IRDETO_UNIT_TEST
int avc_parser_parse_sps(const uint8_t * const buf, size_t size,
                         avc_sps_t * const sps);
#else
static int avc_parser_parse_sps(const uint8_t * const buf, size_t size,
                                avc_sps_t * const sps);
#endif

/**
* @brief    Parse AVC codec extra data in annex b format
* @note     codec extra data comes from AVCodecParameters->extradata
* @param    [in] ctx        codec extra data parser context
* @param    [in] buf        pointer to codec extra data buffer
* @param    [in] size       size of extra data
* @return   0 if success
*           -1 otherwise
*/
static int ff_avc_mp4_parse_extradata_annexb(avc_extra_data_parse_ctx_t
                                             ctx, const uint8_t * buf,
                                             int size);

/**
* @brief    Parse AVC codec extra data in non-annex b format
* @note     codec extra data comes from AVCodecParameters->extradata
*           The data is expected conforms to AVCDecoderConfigurationRecord,
*           Part 15: Advanced Video Coding (AVC) file format, ISO/IEC 14496-15
* @param    [in] ctx        codec extra data parser context
* @param    [in] buf        pointer to codec extra data buffer
* @param    [in] size       size of extra data
* @return   0 if success
*           -1 otherwise
*/
static int ff_avc_mp4_parse_extradata_nonannexb(avc_extra_data_parse_ctx_t
                                                ctx, const uint8_t * buf,
                                                int size);
/**
* @brief    Parse XPS nal
* @note     The input buf should not have annex b start code or nal length in the front, but pure nal data
* @param    [in] ctx        codec extra data parser context
* @param    [in] buf        pointer to single XPS nal
* @param    [in] size       size of XPS
* @return   0 if success
*           -1 otherwise
*/
static int ff_avc_mp4_parse_XPS(avc_extra_data_parse_ctx_t ctx,
                                const uint8_t * buf, int size);

void ff_avc_mp4_parse_extradata_init(avc_extra_data_parse_ctx_t * ctx)
{
    avc_extra_data_ctx_t *parse_ctx =
        av_mallocz(sizeof(avc_extra_data_ctx_t));
    *ctx = parse_ctx;
}

int ff_avc_mp4_parse_extradata(avc_extra_data_parse_ctx_t ctx,
                               const uint8_t * buf, int size)
{
    /**
    * @note since it is known whether the data format is in annexb, so assume annexb first,
    *       if failed, switch to non-annexb parsing (AVCDecoderConfigurationRecord struct)
    */
    if (0 != ff_avc_mp4_parse_extradata_annexb(ctx, buf, size)) {
        return ff_avc_mp4_parse_extradata_nonannexb(ctx, buf, size);
    }
    return 0;
}

/**
*
*/
static int ff_avc_mp4_parse_extradata_nonannexb(avc_extra_data_parse_ctx_t
                                                ctx, const uint8_t * buf,
                                                int size)
{
    avc_extra_data_ctx_t *parse_ctx = (avc_extra_data_ctx_t *) ctx;

    GetBitContext reader;
    if (0 != init_get_bits8(&reader, buf, size)) {
        return -1;
    }
    /**
    * skip configurationVersion, AVCProfileIndication, profile_compatibility,
    * AVCLevelIndication
    */
    skip_bits(&reader, 32);
    skip_bits(&reader, 6);      // reserved 111111
    skip_bits(&reader, 2);      // nal length size
    skip_bits(&reader, 3);      // reserved 111
    const int sps_num = get_bits(&reader, 5);
    int i = 0;
    for (i = 0; i < sps_num; i++) {
        const int size = get_bits(&reader, 16); // size of sps
        if (0 !=
            ff_avc_mp4_parse_XPS(ctx,
                                 reader.buffer +
                                 (get_bits_count(&reader) >> 3), size)) {
            return -1;
        }

        skip_bits(&reader, size << 3);  //skip the sps bits that is already processed
    }


    const int pps_num = get_bits(&reader, 8);
    for (i = 0; i < pps_num; i++) {
        const int size = get_bits(&reader, 16); // size of pps
        if (0 !=
            ff_avc_mp4_parse_XPS(ctx,
                                 reader.buffer +
                                 (get_bits_count(&reader) >> 3), size)) {
            return -1;
        }
        skip_bits(&reader, size << 3);  //skip the sps bits that is already processed
    }
    return 0;
}

/**
*
*/
static int ff_avc_mp4_parse_extradata_annexb(avc_extra_data_parse_ctx_t
                                             ctx, const uint8_t * buf,
                                             int size)
{
    avc_extra_data_ctx_t *parse_ctx = (avc_extra_data_ctx_t *) ctx;
    int idx = 0, prev_idx = 0;
    int start_code_len = 0, found = 0;
    while ((idx + 3) < size) {
        //find 3-byte start code
        if (0 == *(buf + idx) && 0 == *(buf + idx + 1)
            && 1 == *(buf + idx + 2)) {
            start_code_len = 3;
            found = 1;
        }
        //find 4-byte start code
        else if (0 == *(buf + idx) && 0 == *(buf + idx + 1)
                 && 0 == *(buf + idx + 2) && 1 == *(buf + idx + 3)) {
            start_code_len = 4;
            found = 1;
        }

        if (found) {
            int nal_size = idx - prev_idx;
            if (0 != nal_size
                && 0 != ff_avc_mp4_parse_XPS(ctx, buf + prev_idx,
                                             nal_size)) {
                return -1;
            }
            prev_idx = idx + start_code_len;
            found = 0;
            idx += start_code_len;
        } else
            idx++;
    }
    int nal_size = size - prev_idx;
    if (0 != ff_avc_mp4_parse_XPS(ctx, buf + prev_idx, nal_size)) {
        return -1;
    }

    return 0;
}

/**
*
*/
static int ff_avc_mp4_parse_XPS(avc_extra_data_parse_ctx_t ctx,
                                const uint8_t * buf, int size)
{
    int res = 0;
    avc_extra_data_ctx_t *parse_ctx = (avc_extra_data_ctx_t *) ctx;
    if (0 == size) {
        return -1;
    }
    switch (*buf & 0x1f) {
    case AVC_NAL_SPS:
        {
            avc_sps_t sps;
            if (0 != avc_parser_parse_sps(buf, size, &sps)) {
                res = -1;
                break;
            }
            parse_ctx->sps_list[sps.seq_parameter_set_id] = sps;
            break;
        }
    case AVC_NAL_PPS:
        {
            avc_pps_t pps;
            if (0 != avc_parser_parse_pps(buf, size, &pps)) {
                res = -1;
                break;
            }
            // if does not exist
            if (1 !=
                parse_ctx->sps_list[pps.seq_parameter_set_id].exist_flag) {
                res = -1;
                break;
            }
            pps.sps = parse_ctx->sps_list + pps.seq_parameter_set_id;
            parse_ctx->pps_list[pps.pic_parameter_set_id] = pps;
            break;
        }
    default:
        res = -1;
        break;
    }
    return res;
}

/**
*
*/
void ff_avc_mp4_parse_extradata_clean(avc_extra_data_parse_ctx_t ctx)
{
    avc_extra_data_ctx_t *parse_ctx = (avc_extra_data_ctx_t *) ctx;
    av_free(parse_ctx);
}

static const uint8_t *ff_avc_find_startcode_internal(const uint8_t * p,
                                                     const uint8_t * end)
{
    const uint8_t *a = p + 4 - ((intptr_t) p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t *) p;
//      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
//      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080) {     // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p + 1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p + 2;
                if (p[4] == 0 && p[5] == 1)
                    return p + 3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

const uint8_t *ff_avc_find_startcode(const uint8_t * p,
                                     const uint8_t * end)
{
    const uint8_t *out = ff_avc_find_startcode_internal(p, end);
    if (p < out && out < end && !out[-1])
        out--;
    return out;
}

int ff_avc_parse_nal_units(AVIOContext * pb, const uint8_t * buf_in,
                           int size)
{
    const uint8_t *p = buf_in;
    const uint8_t *end = p + size;
    const uint8_t *nal_start, *nal_end;

    size = 0;
    nal_start = ff_avc_find_startcode(p, end);
    for (;;) {
        while (nal_start < end && !*(nal_start++));
        if (nal_start == end)
            break;

        nal_end = ff_avc_find_startcode(nal_start, end);
        avio_wb32(pb, nal_end - nal_start);
        avio_write(pb, nal_start, nal_end - nal_start);
        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }
    return size;
}

int ff_avc_parse_nal_units_buf(const uint8_t * buf_in, uint8_t ** buf,
                               int *size)
{
    AVIOContext *pb;
    int ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
        return ret;

    ff_avc_parse_nal_units(pb, buf_in, *size);

    av_freep(buf);
    *size = avio_close_dyn_buf(pb, buf);
    return 0;
}

int ff_isom_write_avcc(AVIOContext * pb, const uint8_t * data, int len)
{
    AVIOContext *sps_pb = NULL, *pps_pb = NULL;
    uint8_t *buf = NULL, *end, *start = NULL;
    uint8_t *sps = NULL, *pps = NULL;
    uint32_t sps_size = 0, pps_size = 0;
    int ret, nb_sps = 0, nb_pps = 0;

    if (len <= 6)
        return AVERROR_INVALIDDATA;

    /* check for H.264 start code */
    if (AV_RB32(data) != 0x00000001 && AV_RB24(data) != 0x000001) {
        avio_write(pb, data, len);
        return 0;
    }

    ret = ff_avc_parse_nal_units_buf(data, &buf, &len);
    if (ret < 0)
        return ret;
    start = buf;
    end = buf + len;

    ret = avio_open_dyn_buf(&sps_pb);
    if (ret < 0)
        goto fail;
    ret = avio_open_dyn_buf(&pps_pb);
    if (ret < 0)
        goto fail;

    /* look for sps and pps */
    while (end - buf > 4) {
        uint32_t size;
        uint8_t nal_type;
        size = FFMIN(AV_RB32(buf), end - buf - 4);
        buf += 4;
        nal_type = buf[0] & 0x1f;

        if (nal_type == 7) {    /* SPS */
            nb_sps++;
            if (size > UINT16_MAX || nb_sps >= H264_MAX_SPS_COUNT) {
                ret = AVERROR_INVALIDDATA;
                goto fail;
            }
            avio_wb16(sps_pb, size);
            avio_write(sps_pb, buf, size);
        } else if (nal_type == 8) {     /* PPS */
            nb_pps++;
            if (size > UINT16_MAX || nb_pps >= H264_MAX_PPS_COUNT) {
                ret = AVERROR_INVALIDDATA;
                goto fail;
            }
            avio_wb16(pps_pb, size);
            avio_write(pps_pb, buf, size);
        }

        buf += size;
    }
    sps_size = avio_close_dyn_buf(sps_pb, &sps);
    pps_size = avio_close_dyn_buf(pps_pb, &pps);

    if (sps_size < 6 || !pps_size) {
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    avio_w8(pb, 1);             /* version */
    avio_w8(pb, sps[3]);        /* profile */
    avio_w8(pb, sps[4]);        /* profile compat */
    avio_w8(pb, sps[5]);        /* level */
    avio_w8(pb, 0xff);          /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
    avio_w8(pb, 0xe0 | nb_sps); /* 3 bits reserved (111) + 5 bits number of sps */

    avio_write(pb, sps, sps_size);
    avio_w8(pb, nb_pps);        /* number of pps */
    avio_write(pb, pps, pps_size);

  fail:
    if (!sps)
        avio_close_dyn_buf(sps_pb, &sps);
    if (!pps)
        avio_close_dyn_buf(pps_pb, &pps);
    av_free(sps);
    av_free(pps);
    av_free(start);

    return ret;
}

int ff_avc_write_annexb_extradata(const uint8_t * in, uint8_t ** buf,
                                  int *size)
{
    uint16_t sps_size, pps_size;
    uint8_t *out;
    int out_size;

    *buf = NULL;
    if (*size >= 4
        && (AV_RB32(in) == 0x00000001 || AV_RB24(in) == 0x000001))
        return 0;
    if (*size < 11 || in[0] != 1)
        return AVERROR_INVALIDDATA;

    sps_size = AV_RB16(&in[6]);
    if (11 + sps_size > *size)
        return AVERROR_INVALIDDATA;
    pps_size = AV_RB16(&in[9 + sps_size]);
    if (11 + sps_size + pps_size > *size)
        return AVERROR_INVALIDDATA;
    out_size = 8 + sps_size + pps_size;
    out = av_mallocz(out_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!out)
        return AVERROR(ENOMEM);
    AV_WB32(&out[0], 0x00000001);
    memcpy(out + 4, &in[8], sps_size);
    AV_WB32(&out[4 + sps_size], 0x00000001);
    memcpy(out + 8 + sps_size, &in[11 + sps_size], pps_size);
    *buf = out;
    *size = out_size;
    return 0;
}

const uint8_t *ff_avc_mp4_find_startcode(const uint8_t * start,
                                         const uint8_t * end,
                                         int nal_length_size)
{
    unsigned int res = 0;

    if (end - start < nal_length_size)
        return NULL;
    while (nal_length_size--)
        res = (res << 8) | *start++;

    if (res > end - start)
        return NULL;

    return start + res;
}

static int ceil_log2(int val)
{
    int res = 0;
    val--;
    while (val != 0) {
        val >>= 1;
        res++;
    }
    return res;
}

int ff_avc_parser_is_vcl(const uint8_t type)
{
    int result = 0;
    switch (type) {
    case AVC_NAL_SLICE:
    case AVC_NAL_DPA:
    case AVC_NAL_DPB:
    case AVC_NAL_DPC:
    case AVC_NAL_IDR_SLICE:
        result = 1;
        break;
    default:
        result = 0;
    }
    return result;
}


static void avc_parser_skip_scaling_list(GetBitContext * reader,
                                         size_t size)
{
    int32_t last_scale = 8;
    int32_t next_scale = 8;
    int32_t delta_scale;
    size_t j = 0;
    for (j = 0; j < size; j++) {
        if (next_scale != 0) {
            delta_scale = get_se_golomb(reader);
            next_scale = (last_scale + delta_scale) & 0xff;
        }
        last_scale = (next_scale == 0) ? last_scale : next_scale;
    }
}

#ifdef IRDETO_UNIT_TEST
int avc_parser_parse_sps(const uint8_t * const buf, size_t size,
                         avc_sps_t * const sps)
#else
static int avc_parser_parse_sps(const uint8_t * const buf, size_t size,
                                avc_sps_t * const sps)
#endif
{
    int res = -1;
    do {
        GetBitContext reader;
        if (0 != init_get_bits8(&reader, buf, size)) {
            break;
        }

        skip_bits(&reader, 8);  // forbidden_zero_bit, nal_ref_idc, nal_unit_type
        int profile_idc = get_bits(&reader, 8); // profile_idc
        skip_bits(&reader, 8);  // constraint_setX_flag, reserved_zero_2bits
        skip_bits(&reader, 8);  // level_idc
        sps->seq_parameter_set_id = get_ue_golomb(&reader);
        if (sps->seq_parameter_set_id >= MAX_SPS_NUM) {
            break;
        }
        switch (profile_idc) {
        case 100:
        case 110:
        case 122:
        case 244:
        case 44:
        case 83:
        case 86:
        case 118:
        case 128:
        case 138:
        case 139:
        case 134:
        case 135:
            {
                sps->chroma_format_idc = get_ue_golomb(&reader);        // chroma_format_idc
                sps->chroma_array_type = sps->chroma_format_idc;
                if (sps->chroma_format_idc == 3) {
                    sps->separate_colour_plane_flag = get_bits1(&reader);
                    if (sps->separate_colour_plane_flag) {
                        sps->chroma_array_type = 0;
                    }
                }
                get_ue_golomb(&reader); // bit_depth_luma_minus8
                get_ue_golomb(&reader); // bit_depth_chroma_minus8
                skip_bits(&reader, 1);  // qpprime_y_zero_transform_bypass_flag
                uint8_t seq_scaling_matrix_present_flag =
                    get_bits1(&reader);
                if (seq_scaling_matrix_present_flag) {
                    int limit = ((sps->chroma_format_idc != 3) ? 8 : 12);
                    for (size_t i = 0; i < limit; i++) {
                        uint8_t seq_scaling_list_present_flag =
                            get_bits1(&reader);
                        if (seq_scaling_list_present_flag) {
                            if (i < 6) {
                                avc_parser_skip_scaling_list(&reader, 16);
                            } else {
                                avc_parser_skip_scaling_list(&reader, 64);
                            }
                        }
                    }
                }
                break;
            }

        default:
            {
                sps->chroma_format_idc = 1;
                sps->chroma_array_type = 1;
                break;
            }
        }

        sps->log2_max_frame_num = get_ue_golomb(&reader) + 4;
        sps->pic_order_cnt_type = get_ue_golomb(&reader);
        if (0 == sps->pic_order_cnt_type) {
            sps->log2_max_pic_order_cnt_lsb = get_ue_golomb(&reader) + 4;
        } else if (1 == sps->pic_order_cnt_type) {
            sps->delta_pic_order_always_zero_flag = get_bits1(&reader);
            get_ue_golomb(&reader);     // offset_for_non_ref_pic
            get_ue_golomb(&reader);     // offset_for_top_to_bottom_field
            int num_ref_frames_in_pic_order_cnt_cycle =
                get_ue_golomb(&reader);
            for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle;
                 i++) {
                get_ue_golomb(&reader); // offset_for_ref_frame
            }
        }
        get_ue_golomb(&reader); // num_ref_frames
        skip_bits(&reader, 1);  // gaps_in_frame_num_value_allowed_flag
        sps->pic_width_in_mbs = get_ue_golomb(&reader) + 1;
        sps->pic_height_in_map_units = get_ue_golomb(&reader) + 1;
        sps->frame_mbs_only_flag = get_bits1(&reader);
        sps->exist_flag = 1;
        //skip the rest
        res = 0;
    } while (0);
    return res;
}

#ifdef IRDETO_UNIT_TEST
int avc_parser_parse_pps(const uint8_t * const buf, size_t size,
                         avc_pps_t * const pps)
#else
static int avc_parser_parse_pps(const uint8_t * const buf, size_t size,
                                avc_pps_t * const pps)
#endif
{
    int res = -1;
    do {
        GetBitContext reader;
        if (0 != init_get_bits8(&reader, buf, size)) {
            break;
        }

        skip_bits(&reader, 8);  //forbidden_zero_bit, nal_ref_idc, nal_unit_type

        pps->pic_parameter_set_id = get_ue_golomb(&reader);
        if (pps->pic_parameter_set_id >= MAX_PPS_NUM) {
            break;
        }
        pps->seq_parameter_set_id = get_ue_golomb(&reader);
        if (pps->seq_parameter_set_id >= MAX_SPS_NUM) {
            break;
        }
        pps->entropy_coding_mode_flag = get_bits1(&reader);
        pps->bottom_field_pic_order_in_frame_present_flag =
            get_bits1(&reader);
        pps->num_slice_groups_minus1 = get_ue_golomb(&reader);
        if (pps->num_slice_groups_minus1 > 0) {
            pps->slice_group_map_type = get_ue_golomb(&reader);
            if (pps->slice_group_map_type == 0) {
                uint8_t group = 0;
                for (group = 0; group <= pps->num_slice_groups_minus1;
                     group++) {
                    get_ue_golomb(&reader);     // run_length_minus1[group]
                }
            } else if (pps->slice_group_map_type == 2) {
                uint8_t group = 0;
                for (group = 0; group < pps->num_slice_groups_minus1;
                     group++) {
                    get_ue_golomb(&reader);     // top_left[ group ]
                    get_ue_golomb(&reader);     // bottom_right[ group ]
                }
            } else if (pps->slice_group_map_type == 3
                       || pps->slice_group_map_type == 4
                       || pps->slice_group_map_type == 5) {
                skip_bits(&reader, 1);  // slice_group_change_direction_flag
                pps->slice_group_change_rate = get_ue_golomb(&reader) + 1;
            } else if (pps->slice_group_map_type == 6) {
                int pic_size_in_map_units_minus1 = get_ue_golomb(&reader);
                int i = 0;
                for (i = 0; i <= pic_size_in_map_units_minus1; i++) {
                    skip_bits(&reader, ceil_log2(pps->num_slice_groups_minus1 + 1));    // slice_group_id[i]
                }
            }
        }

        pps->num_ref_idx[0] = get_ue_golomb(&reader) + 1;
        pps->num_ref_idx[1] = get_ue_golomb(&reader) + 1;
        pps->weighted_pred_flag = get_bits1(&reader);
        pps->weighted_bipred_idc = get_bits(&reader, 2);
        get_ue_golomb(&reader); // pic_init_qp_minus26
        get_ue_golomb(&reader); // pic_init_qs_minus26
        get_ue_golomb(&reader); // chroma_qp_index_offset
        pps->deblocking_filter_control_present_flag = get_bits1(&reader);
        skip_bits(&reader, 1);  // constrained_intra_pred_flag
        pps->redundant_pic_cnt_present_flag = get_bits1(&reader);
        pps->exist_flag = 1;
        res = 0;
    } while (0);

    return res;
}

static void avc_parser_skip_ref_pic_list_mvc_modification(GetBitContext *
                                                          reader,
                                                          uint8_t const
                                                          slice_type)
{
    int modification_of_pic_nums_idc = 0;
    if (slice_type != AVC_SLICE_I && slice_type != AVC_SLICE_SI) {
        uint8_t ref_pic_list_modification_flag_l0 = get_bits1(reader);
        if (ref_pic_list_modification_flag_l0) {
            do {
                modification_of_pic_nums_idc = get_ue_golomb(reader);
                if (modification_of_pic_nums_idc == 0
                    || modification_of_pic_nums_idc == 1) {
                    get_ue_golomb(reader);      // abs_diff_pic_num_minus1
                } else if (modification_of_pic_nums_idc == 2) {
                    get_ue_golomb(reader);      // long_term_pic_num
                } else if (modification_of_pic_nums_idc == 4
                           || modification_of_pic_nums_idc == 5) {
                    get_ue_golomb(reader);      // abs_diff_view_idx_minus1
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }

    if (slice_type == AVC_SLICE_B) {
        uint8_t ref_pic_list_modification_flag_l1 = get_bits1(reader);
        if (ref_pic_list_modification_flag_l1) {
            do {
                modification_of_pic_nums_idc = get_ue_golomb(reader);
                if (modification_of_pic_nums_idc == 0 ||
                    modification_of_pic_nums_idc == 1) {
                    get_ue_golomb(reader);      // abs_diff_pic_num_minus1
                } else if (modification_of_pic_nums_idc == 2) {
                    get_ue_golomb(reader);      // long_term_pic_num
                } else if (modification_of_pic_nums_idc == 4 ||
                           modification_of_pic_nums_idc == 5) {
                    get_ue_golomb(reader);      // abs_diff_view_idx_minus1
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }
}

static void avc_parser_skip_ref_pic_list_modification(GetBitContext *
                                                      reader,
                                                      uint8_t const
                                                      slice_type)
{
    uint8_t modification_of_pic_nums_idc;

    if (slice_type != AVC_SLICE_I && slice_type != AVC_SLICE_SI) {
        uint8_t ref_pic_list_modification_flag_l0 = get_bits1(reader);
        if (ref_pic_list_modification_flag_l0) {
            do {
                modification_of_pic_nums_idc = get_ue_golomb(reader);
                if (modification_of_pic_nums_idc == 0
                    || modification_of_pic_nums_idc == 1) {
                    get_ue_golomb(reader);      // abs_diff_pic_num_minus1
                } else if (modification_of_pic_nums_idc == 2) {
                    get_ue_golomb(reader);      // long_term_pic_num
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }

    if (slice_type == AVC_SLICE_B) {
        uint8_t ref_pic_list_modification_flag_l1 = get_bits1(reader);
        if (ref_pic_list_modification_flag_l1) {
            do {
                modification_of_pic_nums_idc = get_ue_golomb(reader);
                if (modification_of_pic_nums_idc == 0
                    || modification_of_pic_nums_idc == 1) {
                    get_ue_golomb(reader);      // abs_diff_pic_num_minus1
                } else if (modification_of_pic_nums_idc == 2) {
                    get_ue_golomb(reader);      // long_term_pic_num
                }
            } while (modification_of_pic_nums_idc != 3);
        }
    }
}

static void avc_parser_skip_pred_weight_table(GetBitContext * reader,
                                              uint8_t slice_type,
                                              int *num_ref_idx,
                                              int chroma_array_type)
{
    get_ue_golomb(reader);      // luma_log2_weight_denom
    if (chroma_array_type != 0) {
        get_ue_golomb(reader);  // chroma_log2_weight_denom
    }
    int i = 0;
    for (i = 0; i < num_ref_idx[0]; i++) {
        uint8_t luma_weight_l0_flag = get_bits1(reader);
        if (luma_weight_l0_flag) {
            get_ue_golomb(reader);      // luma_weight_l0[i]
            get_ue_golomb(reader);      // luma_offset_l0[i]
        }
        if (chroma_array_type != 0) {
            uint8_t chroma_weight_l0_flag = get_bits1(reader);
            if (chroma_weight_l0_flag) {
                int j = 0;
                for (j = 0; j < 2; j++) {
                    get_ue_golomb(reader);      // chroma_weight_l0[i][j]
                    get_ue_golomb(reader);      // chroma_offset_l0[i][j]
                }
            }
        }
    }
    if (slice_type == AVC_SLICE_B) {
        int i = 0;
        for (i = 0; i < num_ref_idx[1]; i++) {
            uint8_t luma_weight_l1_flag = get_bits1(reader);
            if (luma_weight_l1_flag) {
                get_ue_golomb(reader);  // luma_weight_l1[i]
                get_ue_golomb(reader);  // luma_offset_l1[i]
            }
            if (chroma_array_type != 0) {
                uint8_t chroma_weight_l1_flag = get_bits1(reader);
                if (chroma_weight_l1_flag) {
                    int j = 0;
                    for (j = 0; j < 2; j++) {
                        get_ue_golomb(reader);  // chroma_weight_l1[i][j]
                        get_ue_golomb(reader);  // chroma_offset_l1[i][j]
                    }
                }
            }
        }
    }
}

static void avc_parser_skip_dec_ref_pic_marking(GetBitContext * reader,
                                                uint8_t nal_unit_type)
{
    if (nal_unit_type == AVC_NAL_IDR_SLICE) {
        skip_bits(reader, 1);   // no_output_of_prior_pics_flag
        skip_bits(reader, 1);   // long_term_reference_flag
    } else {
        uint8_t adaptive_ref_pic_marking_mode_flag = get_bits1(reader);
        if (adaptive_ref_pic_marking_mode_flag) {
            uint8_t memory_management_control_operation = 0;
            do {
                memory_management_control_operation =
                    get_ue_golomb(reader);
                if (1 == memory_management_control_operation
                    || 3 == memory_management_control_operation) {
                    get_ue_golomb(reader);      // difference_of_pic_nums_minus1
                }
                if (2 == memory_management_control_operation) {
                    get_ue_golomb(reader);      // long_term_pic_num
                }
                if (3 == memory_management_control_operation
                    || 6 == memory_management_control_operation) {
                    get_ue_golomb(reader);      // long_term_frame_idx
                }
                if (4 == memory_management_control_operation) {
                    get_ue_golomb(reader);      // max_long_term_frame_idx_plus1
                }
            } while (memory_management_control_operation != 0);
        }
    }
}

int ff_avc_parser_get_slice_header_size(avc_extra_data_parse_ctx_t ctx,
                                        const uint8_t * const buf,
                                        size_t buf_size, size_t * size)
{
    int result = -1;
    avc_extra_data_ctx_t *parse_ctx = (avc_extra_data_ctx_t *) ctx;
    avc_pps_t *pps_list = parse_ctx->pps_list;
    do {
        GetBitContext reader;
        if (0 != init_get_bits8(&reader, buf, buf_size)) {
            break;
        }
        skip_bits(&reader, 1);  // forbidden_zero_bit
        uint8_t nal_ref_idc = get_bits(&reader, 2);     // nal_ref_idc
        uint8_t nal_unit_type = get_bits(&reader, 5);   // nal_unit_type

        get_ue_golomb(&reader); // first_mb_in_slice
        uint8_t slice_type = get_ue_golomb(&reader);
        slice_type %= 5;        //< convert type 5 -9 to identical type 0 - 4
        int pps_id = get_ue_golomb(&reader);
        if (pps_id >= MAX_PPS_NUM) {
            break;
        }
        const avc_pps_t *pps = pps_list + pps_id;
        if (1 != pps->exist_flag) {
            break;
        }
        const avc_sps_t *sps = pps->sps;
        if (1 != sps->exist_flag) {
            break;
        }

        if (1 == sps->separate_colour_plane_flag) {
            skip_bits(&reader, 2);      // colour_plane_id
        }
        skip_bits(&reader, sps->log2_max_frame_num);    // frame_num
        uint8_t field_pic_flag = 0;
        if (!sps->frame_mbs_only_flag) {
            field_pic_flag = get_bits1(&reader);
            if (field_pic_flag) {
                skip_bits(&reader, 1);  // bottom_field_flag
            }
        }

        if (AVC_NAL_IDR_SLICE == nal_unit_type) {
            get_ue_golomb(&reader);     // idr_pic_id
        }

        if (0 == sps->pic_order_cnt_type) {
            skip_bits(&reader, sps->log2_max_pic_order_cnt_lsb);        // pic_order_cnt_lsb
            if (pps->bottom_field_pic_order_in_frame_present_flag
                && !field_pic_flag)
                get_ue_golomb(&reader); // delta_pic_order_cnt_bottom
        }

        if (1 == sps->pic_order_cnt_type
            && !sps->delta_pic_order_always_zero_flag) {
            get_ue_golomb(&reader);     // delta_pic_order_cnt[ 0 ]
            if (pps->bottom_field_pic_order_in_frame_present_flag
                && !field_pic_flag)
                get_ue_golomb(&reader); // delta_pic_order_cnt[ 1 ]
        }

        if (pps->redundant_pic_cnt_present_flag) {
            get_ue_golomb(&reader);     // redundant_pic_cnt
        }

        if (AVC_SLICE_B == slice_type) {
            skip_bits(&reader, 1);      // direct_spatial_mv_pred_flag
        }
        int num_ref_idx[2];
        memcpy(num_ref_idx, pps->num_ref_idx, sizeof(num_ref_idx));
        if (slice_type == AVC_SLICE_P || slice_type == AVC_SLICE_SP
            || slice_type == AVC_SLICE_B) {
            uint8_t num_ref_idx_active_override_flag = get_bits1(&reader);
            if (num_ref_idx_active_override_flag) {
                num_ref_idx[0] = get_ue_golomb(&reader) + 1;
                if (slice_type == AVC_SLICE_B) {
                    num_ref_idx[1] = get_ue_golomb(&reader) + 1;
                }
            }
        }

        if (20 == nal_unit_type || 21 == nal_unit_type) {
            avc_parser_skip_ref_pic_list_mvc_modification(&reader,
                                                          slice_type);
        } else {
            avc_parser_skip_ref_pic_list_modification(&reader, slice_type);
        }
        if ((pps->weighted_pred_flag
             && (slice_type == AVC_SLICE_P || slice_type == AVC_SLICE_SP))
            || (pps->weighted_bipred_idc == 1
                && slice_type == AVC_SLICE_B)) {
            avc_parser_skip_pred_weight_table(&reader, slice_type,
                                              num_ref_idx,
                                              sps->chroma_array_type);
        }

        if (0 != nal_ref_idc) {
            avc_parser_skip_dec_ref_pic_marking(&reader, nal_unit_type);
        }

        if (pps->entropy_coding_mode_flag && slice_type != AVC_SLICE_I
            && slice_type != AVC_SLICE_SI) {
            get_ue_golomb(&reader);     // cabac_init_idc
        }

        get_ue_golomb(&reader); // slice_qp_delta
        if (slice_type == AVC_SLICE_SP || slice_type == AVC_SLICE_SI) {
            if (slice_type == AVC_SLICE_SP) {
                skip_bits(&reader, 1);  // sp_for_switch_flag
            }
            get_ue_golomb(&reader);     // slice_qs_delta
        }

        if (pps->deblocking_filter_control_present_flag) {
            int disable_deblocking_filter_idc = get_ue_golomb(&reader);
            if (disable_deblocking_filter_idc != 1) {
                get_ue_golomb(&reader); // slice_alpha_c0_offset_div2
                get_ue_golomb(&reader); // slice_beta_offset_div2
            }
        }
        if (pps->num_slice_groups_minus1 > 0 &&
            pps->slice_group_map_type >= 3
            && pps->slice_group_map_type <= 5) {
            int pic_size_in_map_units =
                sps->pic_height_in_map_units * sps->pic_width_in_mbs;
            int len =
                (pic_size_in_map_units +
                 pps->slice_group_change_rate) /
                (pps->slice_group_change_rate - 1);
            len = ceil_log2(len + 1);
            skip_bits(&reader, len);    // slice_group_change_cycle
        }
        int processed_bits = get_bits_count(&reader);
        *size =
            (processed_bits % 8 ==
             0) ? (processed_bits >> 3) : ((processed_bits >> 3) + 1);
        result = 0;
    } while (0);
    return result;
}

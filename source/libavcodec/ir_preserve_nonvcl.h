#ifndef AVCODEC_IR_LIBX264_PRESERVE_NONVCL_H
#define AVCODEC_IR_LIBX264_PRESERVE_NONVCL_H

#include "irxps/ir_xps_common.h"
#include "x264.h"
#include "x265.h"
#include "avcodec.h"

// codecs
typedef enum
{
    CODEC_AVC = 0,
    CODEC_HEVC,
    IR_XVC_CODECS_SIZE
} ir_xvc_codecs;

// these are "private" typedefs, not intended to be used in the unit including this header
typedef int (*is_vcl)(int nal_type);
typedef int (*is_pps)(int nal_type);
typedef int (*get_nal_type)(uint8_t hdr);
typedef int (*zero_byte)(int nal_type);
typedef void (*get_nal_params)(void *nal, int idx, uint8_t **buf, int *size, int *type);

// AnnexB vs avcc/hvcc parsing
typedef int (*get_nal)(uint8_t *p, uint8_t *p_end, int *nal_type, int *nal_len, ir_xvc_codecs codec);

/**
* @brief check if given nal_type value is x264 vcl
*/
static inline int is_x264_vcl(int nal_type)
{
    return (nal_type >= NAL_SLICE && nal_type <= NAL_SLICE_IDR) ? 1 : 0;
}

/**
* @brief check if given nal_type value is x264 sps or pps
*/
static inline int is_x264_sps_pps(int nal_type)
{
    return (nal_type == NAL_SPS || nal_type == NAL_PPS) ? 1 : 0;
}

/**
* @brief check if given nal_type value is x264 pps
*/
static inline int is_x264_pps(int nal_type)
{
    return (nal_type == NAL_PPS) ? 1 : 0;
}

/**
* @brief check if given nal_type value is x265 vcl
*/
static inline int is_x265_vcl(int nal_type)
{
    return (nal_type >= 0 && nal_type <= 31) ? 1 : 0;
}

/**
* @brief check if given nal_type value is x265 sps or pps
*/
static inline int is_x265_vps_sps_pps(int nal_type)
{
    return (nal_type >= NAL_UNIT_VPS && nal_type <= NAL_UNIT_PPS) ? 1 : 0;
}

/**
* @brief check if given nal_type value is x265 pps
*/
static inline int is_x265_pps(int nal_type)
{
    return (nal_type == NAL_UNIT_PPS) ? 1 : 0;
}

/**
* @brief get the nalu type from the 1st header byte for avc type streams.
*/
static inline int ir_get_x264_nal_type(uint8_t hdr)
{
    return hdr & 0x1f;
}

/**
* @brief get the nalu type from the 1st header byte for hevc type streams.
*/
static inline int ir_get_x265_nal_type(uint8_t hdr)
{
    return (hdr >> 1) & 0x3f;
}

/**
* @brief get the buffer, size and type of a nalu of type x265_nal at index idx.
*/
static inline void ir_get_x265_nal_params(void *nal, int idx, uint8_t **buf, int *size, int *type)
{
    x265_nal *x265_nal = nal;
    *buf  = x265_nal[idx].payload;
    *size = x265_nal[idx].sizeBytes;
    *type = x265_nal[idx].type;
}

/**
* @brief get the buffer, size and type of a nalu of type x264_nal_t at index idx.
*/
static inline void ir_get_x264_nal_params(void *nal, int idx, uint8_t **buf, int *size, int *type)
{
    x264_nal_t *x264_nal = nal;
    *buf  = x264_nal[idx].p_payload;
    *size = x264_nal[idx].i_payload;
    *type = x264_nal[idx].i_type;
}


// lookup for parsing functions
static const is_vcl fn_is_vcl[IR_XVC_CODECS_SIZE] = {is_x264_vcl, is_x265_vcl};
static const is_pps fn_is_pps[IR_XVC_CODECS_SIZE] = {is_x264_pps, is_x265_pps};
static const zero_byte fn_four_bytes_startcode[IR_XVC_CODECS_SIZE] = {is_x264_sps_pps, is_x265_vps_sps_pps};
static const get_nal_type fn_get_nal_type[IR_XVC_CODECS_SIZE] = {ir_get_x264_nal_type, ir_get_x265_nal_type};
static const get_nal_params fn_get_nal_params[IR_XVC_CODECS_SIZE] = {ir_get_x264_nal_params, ir_get_x265_nal_params};


/**
 * @brief           Get NAL unit start in the buffer
 * @param p         Address of a pointer to the buffer. On successs, the address of the first byte of the start
 *                  code found else equal to p_end.
 * @param p_end     Points one position beyond the last element of the buffer.
 * @codec           The type of the codec, Valid values are CODEC_AVC and CODEC_HEVC.
 * @return          0 on succes, otherwise failure.
 */
static inline int ir_get_nal_start(uint8_t **p, uint8_t *p_end, ir_xvc_codecs codec)
{
    uint8_t *p_tmp;
    int zero_cnt = 0;
    int found = 0;
    int nal_type;

    for (p_tmp = *p; p_tmp < p_end; p_tmp++) {
        if (zero_cnt >= 2 && (*p_tmp) == 0x1 && p_tmp + 1 < p_end) {
            found = 1;
            break;
        }
        zero_cnt = ((*p_tmp) == 0x0) ? zero_cnt + 1 : 0;
    }

    if (found) {
        nal_type = fn_get_nal_type[codec](p_tmp[1]);
        *p = p_tmp - (zero_cnt > 2 && fn_four_bytes_startcode[codec](nal_type) ? 3 : 2);
    } else {
        *p = p_end;
    }

    return found ? 0 : 1;
}

/**
 * @brief           Searches buffer p for the start of a annexb formatted nal and returns its length and type.
 * @param p         Pointer to the buffer.
 * @param p_end     Points one position beyond the last element of the buffer.
 * @param nal_type  Returns the type of the nal found.
 * @param nal_len   Returns the length of the nal found.
 * @param codec     The type of the codec, Valid values are CODEC_AVC and CODEC_HEVC.
 * @return          0 if NAL type and length are properly determined. 1 otherwise, e.g. when
 *                  not enough data available as required by format specification
 */
static inline int ir_get_nal_annexb(uint8_t *p, uint8_t *p_end, int *nal_type, int *nal_len, ir_xvc_codecs codec)
{
    uint8_t *p_start = p;

    if (p == p_end)
    {
        // exit early as already reached end by pointers
        return 1;
    }
    if  (ir_get_nal_start(&p_start, p_end, codec) != 0)
    {
        // no corresponding start code can be found, thus no valid NAL present
        return 1;
    }

    // start code size: when 00 00 00 01 Annex B start code is present, 3rd byte is 0
    size_t start_code_size = (p_start[2] == 0x0) ? 4 : 3;
    // obtain pointer to byte with NAL type and validate it
    uint8_t *p_nal_type = p_start + start_code_size;
    if (p_nal_type >= p_end)
    {
        return 1; // no valid NAL anymore
    }
    // retrieve NAL type
    *nal_type = fn_get_nal_type[codec](*p_nal_type);

    p_start = p_nal_type; // Skip NAL start of current, before going to search for the next nal start
    ir_get_nal_start(&p_start, p_end, codec);
    // regardless of next NAL start code is found or not, the remaining data determines NAL size.
    // Also, nal_len will include the number of zero bytes potentially present in front of the first NAL.
    *nal_len = p_start - p;

    return 0;
}

/**
 * @brief           Returns the length and type of an avcc/hvcc formatted nal in buffer p;
 * @param p         Pointer to the buffer.
 * @param p_end     Points one position beyond the last element of the buffer.
 * @param nal_type  Returns the type of the nal found.
 * @param nal_len   Returns the length of the nal found.
 * @param codec     The tyoe of the codec, Valid values are CODEC_AVC and CODEC_HEVC.
 * @return          0 if NAL type and length are properly determined. 1 otherwise, e.g. when
 *                  not enough data available as required by format specification
 */
static inline int ir_get_nal_avcc_hvcc(uint8_t *p, uint8_t *p_end, int *nal_type, int *nal_len, ir_xvc_codecs codec)
{
    int len = 0;
    int i;

    if (p + 4 >= p_end) {
        return 1;
    }
    for (i = 0; i < 4; i++) {
        len = (len << 8) | p[i];
    }
    if (p + len + 4 > p_end) {
        return 1;
    }

    *nal_type = fn_get_nal_type[codec](p[4]);
    *nal_len = len + 4;

    return 0;
}

/**
 * @brief               Merge original AU's non-VCL NAL units into output x264/x265 AU. Supports both AnnexB
 *                      and avcc/hvcc formatted streams.
 * @param xps_context   A pointer to the ir_xps_context structure that holds the information that is exported by the
 *                      decoder via the opaque field of the AVframe. Its meta data field provides a pointer to the
 *                      original AVPacket carrying the access unit (AU) of the source stream.
 * @param pkt           The AVPacket that will hold the result of the merged src- and re-encoded AU's.
 * @param nal           A pointer to the re-encoded AU represented by an array of nal structures. The type of the
 *                      nal structure depends on parameter codec, either x264_nal_t or x265_nal.
 * @param nnal          The number of nalus in the re-encoded AU.
 * @param is_annexb     Specifies the format of the stream. In case is_annexB equals zero, avcc/hvcc is selected,
 *                      else AnnexB.
 * @param codec         The type of codec. Valid values are CODEC_AVC and CODEC_HEVC.
 */
static inline void ir_merge_pkt_nonvcl(ir_xps_context* xps_context, AVPacket *pkt,
        void *nal, int nnal, int is_annexb, ir_xvc_codecs codec)
{
    // Get a pointer to the original AVPacket from the xps export structure
    AVPacket *pkt_src;
    switch (codec)
    {
        case CODEC_AVC:
            pkt_src = xps_context->avc_meta.pkt;
            break;
        case CODEC_HEVC:
        default:
            pkt_src = xps_context->hevc_meta.pkt;
            break;
    }

    uint8_t *p_src = pkt_src->data;
    uint8_t *p_src_end = pkt_src->data + pkt_src->size;
    uint8_t *p_dst_buf = malloc(pkt_src->size + pkt->size + 1); // Output size will not exceed combined AU sizes.
    uint8_t *p_dst = p_dst_buf;

    // Boolean flag to keep indication if the re-encoded AU VCL NALs were already copied in the loop below
    int reenc_copied = 0;

    // main loop to iterate source AVPacket payload and merge with re-encoded AU payload. Non-VCL NALs
    // are copied to the output buffer from source buffer, VCL NALs are replaced with the
    // PPS NALs and VCL NALs from the re-encoded AU.
    do {
        int nal_type, nal_len = 0;
        // retrieve source NAL details: type and length. The function used depends on format type.
        get_nal fn_get_nal = is_annexb ? ir_get_nal_annexb : ir_get_nal_avcc_hvcc;
        if (fn_get_nal(p_src, p_src_end, &nal_type, &nal_len, codec) > 0) {
            // not able to properly retrieve NAL details, must stop now
            break;
        }

        if (fn_is_vcl[codec](nal_type)) {
            // VCL NALs handling here
            if (!reenc_copied) {
                // When arrived at the first VCL of the source buffer, copy the re-encoded AU NALs (PPS and VCL)
                for (int i = 0; i < nnal; i++) {
                    uint8_t* p_enc;
                    int nal_type_enc, nal_len_enc;
                    // Get the buffer pointer and length of the nal to be copied
                    fn_get_nal_params[codec](nal, i, &p_enc, &nal_len_enc, &nal_type_enc);
                    // As mentioned above, only PPS and VCL NALs are copied to the output.
                    if (fn_is_pps[codec](nal_type_enc) || fn_is_vcl[codec](nal_type_enc)) {
                        memcpy(p_dst, p_enc, nal_len_enc);
                        p_dst += nal_len_enc;
                    }
                }
                reenc_copied = 1; // Done copying the re-encoded AU NALs to the output.
            }
            else {
                // skip VCL NAL, copied already because reenc_copied flag set
                p_src += nal_len;
            }
        }
        else {
            // non-VCL NALs handling: copy any of them from the source AU buffer
            memcpy(p_dst, p_src, nal_len);
            p_dst += nal_len;
            p_src += nal_len;
        }
    } while (1);

    // Adjust the size of the output packet and copy the output buffer to the buffer of the output packet.
    int new_size = p_dst - p_dst_buf;
    if (new_size > pkt->size) {
        av_grow_packet(pkt, new_size - pkt->size);
    }
    else  {
        av_shrink_packet(pkt, new_size);
    }
    memcpy(pkt->data, p_dst_buf, new_size);

    free(p_dst_buf);
}

#endif //AVCODEC_IR_LIBX264_PRESERVE_NONVCL_H

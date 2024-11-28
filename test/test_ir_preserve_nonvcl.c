#include <check.h>

#include "libavcodec/ir_preserve_nonvcl.h"
#include "irxps/ir_xps_export_ref.h"

// AVC nalu types representing a category
#define AVC_PRE (NAL_SEI)
#define AVC_VCL (NAL_SLICE)
#define AVC_SUF (NAL_FILLER)
#define AVC_PPS (NAL_PPS)

// HEVC nalu types representing a category.
// Note: shifted left by 1 for implementation convenience
#define HEVC_PRE (NAL_UNIT_PREFIX_SEI << 1)
#define HEVC_VCL (NAL_UNIT_CODED_SLICE_TRAIL_N << 1)
#define HEVC_SUF (NAL_UNIT_SUFFIX_SEI << 1)
#define HEVC_PPS (NAL_UNIT_PPS << 1)


/**
 * @brief Setup test arguments required to be passed to ir_merge_pkt_nonvcl() function
 * @param pkt  AVPacket pointer
 * @param p_xps_context pointer to ir_xps_context*
 * @param src source payload buffer
 * @param src_len source payload buffer size
 * @param codec AVC or HEVC
 */
static void setup_test_args(AVPacket *pkt, ir_xps_context **p_xps_context, uint8_t *src, int src_len, ir_xvc_codecs codec)
{
    *p_xps_context = ir_xps_context_create();
    switch (codec)
    {
        case CODEC_AVC:
            (*p_xps_context)->avc_meta.pkt = pkt;
            break;
        case CODEC_HEVC:
        default:
            (*p_xps_context)->hevc_meta.pkt = pkt;
            break;
    }
    av_new_packet(pkt, src_len);
    memcpy(pkt->data, src, src_len);
}

/**
 * @brief Teardown (cleanup) test arguments
 * @param pkt AVPacket pointer
 * @param xps_context ir_xps_context pointer
 */
static void teardown_test_args(AVPacket *pkt, ir_xps_context *xps_context)
{
    av_packet_unref(pkt);
    xps_context->avc_meta.pkt = NULL;
    xps_context->hevc_meta.pkt = NULL;
    ir_xps_context_destroy(xps_context);
}


START_TEST(test_is_x264_vcl)
{
    int res = is_x264_vcl(NAL_SLICE);
    fail_unless(1 == res);
}
END_TEST

START_TEST(test_is_x264_sps_pps)
{
    int res = is_x264_sps_pps(NAL_SLICE);
    fail_unless(0 == res);
}
END_TEST

START_TEST(test_is_x264_pps)
{
    int res = is_x264_pps(NAL_SLICE);
    fail_unless(0 == res);

    res = is_x264_pps(NAL_PPS);
    fail_unless(1 == res);
}
END_TEST

START_TEST(test_is_x265_vcl)
{
    int res = is_x265_vcl(NAL_UNIT_CODED_SLICE_TRAIL_N);
    fail_unless(1 == res);
}
END_TEST

START_TEST(test_is_x265_vps_sps_pps)
{
    int res = is_x265_vps_sps_pps(NAL_UNIT_CODED_SLICE_TRAIL_N);
    fail_unless(0 == res);
}
END_TEST

START_TEST(test_is_x265_pps)
{
    int res = is_x265_pps(NAL_UNIT_CODED_SLICE_TRAIL_N);
    fail_unless(0 == res);

    res = is_x265_pps(NAL_UNIT_PPS);
    fail_unless(1 == res);
}
END_TEST


uint8_t annexb_avc_pps[] = {0,0,0,1,AVC_PPS,100};
uint8_t annexb_nal_vcl[] = {0,0,1,AVC_VCL,101};
x264_nal_t annexb_avc_nal[2] = {
        { .p_payload = annexb_avc_pps, .i_payload = 6, .i_type = AVC_PPS },
        { .p_payload = annexb_nal_vcl, .i_payload = 5, .i_type = AVC_VCL }
};

START_TEST(test_merge_avc_annexb_prefix)
{
    // input buffer and expected merged buffer
    uint8_t src_prefix[] = {0,0,0,1,AVC_PRE,10,0,0,1,AVC_VCL,11};
    uint8_t mrg_prefix[] = {0,0,0,1,AVC_PRE,10,0,0,0,1,AVC_PPS,100,0,0,1,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_prefix, 11, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_avc_nal, 2, 1, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 17);
    fail_unless(0 == memcmp(pkt.data, mrg_prefix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_annexb_vclonly)
{
    // input buffer and expected merged buffer
    uint8_t src_vclonly[] = {0,0,1,AVC_VCL,11};
    uint8_t mrg_vclonly[] = {0,0,0,1,AVC_PPS,100,0,0,1,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_vclonly, 5, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_avc_nal, 2, 1, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 11);
    fail_unless(0 == memcmp(pkt.data, mrg_vclonly, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_annexb_suffix)
{
    // input buffer and expected merged buffer
    uint8_t src_suffix[] = {0,0,0,1,AVC_VCL,10,0,0,1,AVC_SUF,11};
    uint8_t mrg_suffix[] = {0,0,0,1,AVC_PPS,100,0,0,1,AVC_VCL,101,0,0,1,AVC_SUF,11};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_suffix, 11, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_avc_nal, 2, 1, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 16);
    fail_unless(0 == memcmp(pkt.data, mrg_suffix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_annexb_mix)
{
    // input buffer and expected merged buffer
    uint8_t src_mix[] = {0,0,0,1,AVC_PPS,10,0,0,1,AVC_PRE,11,0,0,1,AVC_VCL,12,0,0,1,AVC_SUF,13};
    uint8_t mrg_mix[] = {0,0,0,1,AVC_PPS,10,0,0,1,AVC_PRE,11,0,0,0,1,AVC_PPS,100,0,0,1,AVC_VCL,101,0,0,1,AVC_SUF,13};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_mix, 21, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_avc_nal, 2, 1, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 27);
    fail_unless(0 == memcmp(pkt.data, mrg_mix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_annexb_zero_padding)
{
    // input buffer and expected merged buffer
    uint8_t src_zeros[] = {0,0,0,1,AVC_PRE,10,0,0,0,0,0,0,0,0,0,1,AVC_VCL,11};
    uint8_t mrg_zeros[] = {0,0,0,1,AVC_PRE,10,0,0,0,0,0,0,0,0,0,0,1,AVC_PPS,100,0,0,1,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_zeros, 18, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_avc_nal, 2, 1, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 24);
    fail_unless(0 == memcmp(pkt.data, mrg_zeros, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_annexb_3byte_startcode)
{
    // input buffer and expected merged buffer
    uint8_t src_3byte[] = {0,0,1,AVC_PRE,10,0,0,1,AVC_PPS,11,0,0,1,AVC_VCL,12};
    uint8_t mrg_3byte[] = {0,0,1,AVC_PRE,10,0,0,1,AVC_PPS,11,0,0,0,1,AVC_PPS,100,0,0,1,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_3byte, 15, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_avc_nal, 2, 1, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 21);
    fail_unless(0 == memcmp(pkt.data, mrg_3byte, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST


uint8_t avcc_nal_pps[] = {0,0,0,2,AVC_PPS,100};
uint8_t avcc_nal_vcl[] = {0,0,0,2,AVC_VCL,101};
x264_nal_t avcc_nal[2] = {
        { .p_payload = avcc_nal_pps, .i_payload = 6, .i_type = AVC_PPS },
        { .p_payload = avcc_nal_vcl, .i_payload = 6, .i_type = AVC_VCL }
};

START_TEST(test_merge_avc_avcc_prefix)
{
    // input buffer and expected merged buffer
    uint8_t src_prefix[] = {0,0,0,2,AVC_PRE,10,0,0,0,2,AVC_VCL,11};
    uint8_t mrg_prefix[] = {0,0,0,2,AVC_PRE,10,0,0,0,2,AVC_PPS,100,0,0,0,2,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_prefix, 12, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, avcc_nal, 2, 0, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 18);
    fail_unless(0 == memcmp(pkt.data, mrg_prefix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_avcc_vclonly)
{
    // input buffer and expected merged buffer
    uint8_t src_vclonly[] = {0,0,0,2,AVC_VCL,11};
    uint8_t mrg_vclonly[] = {0,0,0,2,AVC_PPS,100,0,0,0,2,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_vclonly, 6, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, avcc_nal, 2, 0, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 12);
    fail_unless(0 == memcmp(pkt.data, mrg_vclonly, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_avcc_suffix)
{
    // input buffer and expected merged buffer
    uint8_t src_suffix[] = {0,0,0,2,AVC_VCL,10,0,0,0,2,AVC_SUF,11};
    uint8_t mrg_suffix[] = {0,0,0,2,AVC_PPS,100,0,0,0,2,AVC_VCL,101,0,0,0,2,AVC_SUF,11};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_suffix, 12, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, avcc_nal, 2, 0, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 18);
    fail_unless(0 == memcmp(pkt.data, mrg_suffix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_avcc_mix)
{
    // input buffer and expected merged buffer
    uint8_t src_mix[] = {0,0,0,2,AVC_PPS,10,0,0,0,2,AVC_PRE,11,0,0,0,2,AVC_VCL,12,0,0,0,2,AVC_SUF,13};
    uint8_t mrg_mix[] = {0,0,0,2,AVC_PPS,10,0,0,0,2,AVC_PRE,11,0,0,0,2,AVC_PPS,100,0,0,0,2,AVC_VCL,101,0,0,0,2,AVC_SUF,13};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_mix, 24, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, avcc_nal, 2, 0, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 30);
    fail_unless(0 == memcmp(pkt.data, mrg_mix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_avc_avcc_zero_padding)
{
    // input buffer and expected merged buffer
    uint8_t src_zeros[] = {0,0,0,9,AVC_PRE,10,0,0,0,0,0,0,0, 0,0,0,2,AVC_VCL,11};
    uint8_t mrg_zeros[] = {0,0,0,9,AVC_PRE,10,0,0,0,0,0,0,0, 0,0,0,2,AVC_PPS,100,0,0,0,2,AVC_VCL,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_zeros, 19, CODEC_AVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, avcc_nal, 2, 0, CODEC_AVC);

    // assert
    fail_unless(pkt.size == 25);
    fail_unless(0 == memcmp(pkt.data, mrg_zeros, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST


uint8_t annexb_hevc_nal_pps[] = {0,0,0,1,HEVC_PPS,0,100};
uint8_t annexb_hevc_nal_vcl[] = {0,0,1,HEVC_VCL,0,101};
x265_nal annexb_hevc_nal[2] = {
        { .payload = annexb_hevc_nal_pps, .sizeBytes = 7, .type = HEVC_PPS >> 1 },
        { .payload = annexb_hevc_nal_vcl, .sizeBytes = 6, .type = HEVC_VCL >> 1 }
};

START_TEST(test_merge_hevc_annexb_prefix)
{
    // input buffer and expected merged buffer
    uint8_t src_prefix[] = {0,0,0,1,HEVC_PRE,0,10,0,0,1,HEVC_VCL,0,11};
    uint8_t mrg_prefix[] = {0,0,0,1,HEVC_PRE,0,10,0,0,0,1,HEVC_PPS,0,100,0,0,1,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_prefix, 13, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_hevc_nal, 2, 1, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 20);
    fail_unless(0 == memcmp(pkt.data, mrg_prefix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_annexb_vclonly)
{
    // input buffer and expected merged buffer
    uint8_t src_vclonly[] = {0,0,0,1,HEVC_VCL,0,11};
    uint8_t mrg_vclonly[] = {0,0,0,1,HEVC_PPS,0,100,0,0,1,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_vclonly, 7, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_hevc_nal, 2, 1, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 13);
    fail_unless(0 == memcmp(pkt.data, mrg_vclonly, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_annexb_suffix)
{
    // input buffer and expected merged buffer
    uint8_t src_suffix[] = {0,0,0,1,HEVC_VCL,0,10,0,0,1,HEVC_SUF,0,11};
    uint8_t mrg_suffix[] = {0,0,0,1,HEVC_PPS,0,100,0,0,1,HEVC_VCL,0,101,0,0,1,HEVC_SUF,0,11};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_suffix, 13, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_hevc_nal, 2, 1, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 19);
    fail_unless(0 == memcmp(pkt.data, mrg_suffix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_annexb_mix)
{
    // input buffer and expected merged buffer
    uint8_t src_mix[] = {0,0,0,1,HEVC_PPS,0,10,0,0,1,HEVC_PRE,0,11,0,0,1,HEVC_VCL,0,12,0,0,1,HEVC_SUF,0,13};
    uint8_t mrg_mix[] = {0,0,0,1,HEVC_PPS,0,10,0,0,1,HEVC_PRE,0,11,0,0,0,1,HEVC_PPS,0,100,0,0,1,HEVC_VCL,0,101,0,0,1,HEVC_SUF,0,13};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_mix, 25, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_hevc_nal, 2, 1, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 32);
    fail_unless(0 == memcmp(pkt.data, mrg_mix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_annexb_zero_padding)
{
    // input buffer and expected merged buffer
    uint8_t src_zeros[] = {0,0,0,1,HEVC_PRE,0,10,0,0,0,0,0,0,0,0,0,1,HEVC_VCL,0,11};
    uint8_t mrg_zeros[] = {0,0,0,1,HEVC_PRE,0,10,0,0,0,0,0,0,0,0,0,0,1,HEVC_PPS,0,100,0,0,1,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_zeros, 20, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_hevc_nal, 2, 1, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 27);
    fail_unless(0 == memcmp(pkt.data, mrg_zeros, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST


START_TEST(test_merge_hevc_annexb_3byte_startcode)
{
    // input buffer and expected merged buffer
    uint8_t src_3byte[] = {0,0,1,HEVC_PRE,0,10,0,0,1,HEVC_PPS,0,11,0,0,1,HEVC_VCL,0,12};
    uint8_t mrg_3byte[] = {0,0,1,HEVC_PRE,0,10,0,0,1,HEVC_PPS,0,11,0,0,0,1,HEVC_PPS,0,100,0,0,1,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_3byte, 18, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, annexb_hevc_nal, 2, 1, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 25);
    fail_unless(0 == memcmp(pkt.data, mrg_3byte, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

uint8_t hvcc_nal_pps[] = {0,0,0,3,HEVC_PPS,0,100};
uint8_t hvcc_nal_vcl[] = {0,0,0,3,HEVC_VCL,0,101};
x265_nal hvcc_nal[2] = {
        { .payload = hvcc_nal_pps, .sizeBytes = 7, .type = HEVC_PPS >> 1 },
        { .payload = hvcc_nal_vcl, .sizeBytes = 7, .type = HEVC_VCL >> 1 }
};

START_TEST(test_merge_hevc_hvcc_prefix)
{
    // input buffer and expected merged buffer
    uint8_t src_prefix[] = {0,0,0,3,HEVC_PRE,0,10,0,0,0,3,HEVC_VCL,0,11};
    uint8_t mrg_prefix[] = {0,0,0,3,HEVC_PRE,0,10,0,0,0,3,HEVC_PPS,0,100,0,0,0,3,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_prefix, 14, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, hvcc_nal, 2, 0, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 21);
    fail_unless(0 == memcmp(pkt.data, mrg_prefix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_hvcc_vclonly)
{
    // input buffer and expected merged buffer
    uint8_t src_vclonly[] = {0,0,0,3,HEVC_VCL,0,11};
    uint8_t mrg_vclonly[] = {0,0,0,3,HEVC_PPS,0,100,0,0,0,3,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_vclonly, 7, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, hvcc_nal, 2, 0, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 14);
    fail_unless(0 == memcmp(pkt.data, mrg_vclonly, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_hvcc_suffix)
{
    // input buffer and expected merged buffer
    uint8_t src_suffix[] = {0,0,0,3,HEVC_VCL,0,10,0,0,0,3,HEVC_SUF,0,11};
    uint8_t mrg_suffix[] = {0,0,0,3,HEVC_PPS,0,100,0,0,0,3,HEVC_VCL,0,101,0,0,0,3,HEVC_SUF,0,11};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_suffix, 14, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, hvcc_nal, 2, 0, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 21);
    fail_unless(0 == memcmp(pkt.data, mrg_suffix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_hvcc_mix)
{
    // input buffer and expected merged buffer
    uint8_t src_mix[] = {0,0,0,3,HEVC_PPS,0,10,0,0,0,3,HEVC_PRE,0,11,0,0,0,3,HEVC_VCL,0,12,0,0,0,3,HEVC_SUF,0,13};
    uint8_t mrg_mix[] = {0,0,0,3,HEVC_PPS,0,10,0,0,0,3,HEVC_PRE,0,11,0,0,0,3,HEVC_PPS,0,100,0,0,0,3,HEVC_VCL,0,101,0,0,0,3,HEVC_SUF,0,13};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_mix, 28, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, hvcc_nal, 2, 0, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 35);
    fail_unless(0 == memcmp(pkt.data, mrg_mix, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST

START_TEST(test_merge_hevc_hvcc_zero_pading)
{
    // input buffer and expected merged buffer
    uint8_t src_zeros[] = {0,0,0,10,HEVC_PRE,0,10,0,0,0,0,0,0,0,0,0,0,3,HEVC_VCL,0,11};
    uint8_t mrg_zeros[] = {0,0,0,10,HEVC_PRE,0,10,0,0,0,0,0,0,0,0,0,0,3,HEVC_PPS,0,100,0,0,0,3,HEVC_VCL,0,101};

    // setup
    AVPacket pkt;
    ir_xps_context* xps_context;
    setup_test_args(&pkt, &xps_context, src_zeros, 21, CODEC_HEVC);

    // call tested function
    ir_merge_pkt_nonvcl(xps_context, &pkt, hvcc_nal, 2, 0, CODEC_HEVC);

    // assert
    fail_unless(pkt.size == 28);
    fail_unless(0 == memcmp(pkt.data, mrg_zeros, pkt.size));

    // teardown
    teardown_test_args(&pkt, xps_context);
}
END_TEST


Suite *test_suite(void)
{
    Suite *s = suite_create("Irdeto patches: preserve non-VCL NAL units from the original AVC/HEVC AU into re-encoded AVC/HEVC AU");
    TCase *tc = tcase_create("AVC NAL type verification functions tests");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_is_x264_vcl);
    tcase_add_test(tc, test_is_x264_sps_pps);
    tcase_add_test(tc, test_is_x264_pps);
    TCase *tc_2 = tcase_create("HEVC NAL type verification functions tests");
    suite_add_tcase(s, tc_2);
    tcase_add_test(tc_2, test_is_x265_vcl);
    tcase_add_test(tc_2, test_is_x265_vps_sps_pps);
    tcase_add_test(tc_2, test_is_x265_pps);
    TCase *tc_3 = tcase_create("AVC non-VCL NAL units preservation/merging tests");
    suite_add_tcase(s, tc_3);
    tcase_add_test(tc_3, test_merge_avc_annexb_prefix);
    tcase_add_test(tc_3, test_merge_avc_annexb_suffix);
    tcase_add_test(tc_3, test_merge_avc_annexb_vclonly);
    tcase_add_test(tc_3, test_merge_avc_annexb_mix);
    tcase_add_test(tc_3, test_merge_avc_annexb_zero_padding);
    tcase_add_test(tc_3, test_merge_avc_annexb_3byte_startcode);
    tcase_add_test(tc_3, test_merge_avc_avcc_prefix);
    tcase_add_test(tc_3, test_merge_avc_avcc_suffix);
    tcase_add_test(tc_3, test_merge_avc_avcc_vclonly);
    tcase_add_test(tc_3, test_merge_avc_avcc_mix);
    tcase_add_test(tc_3, test_merge_avc_avcc_zero_padding);
    TCase *tc_4 = tcase_create("HEVC non-VCL NAL units preservation/merging tests");
    suite_add_tcase(s, tc_4);
    tcase_add_test(tc_4, test_merge_hevc_annexb_prefix);
    tcase_add_test(tc_4, test_merge_hevc_annexb_suffix);
    tcase_add_test(tc_4, test_merge_hevc_annexb_vclonly);
    tcase_add_test(tc_4, test_merge_hevc_annexb_mix);
    tcase_add_test(tc_4, test_merge_hevc_annexb_zero_padding);
    tcase_add_test(tc_3, test_merge_hevc_annexb_3byte_startcode);
    tcase_add_test(tc_4, test_merge_hevc_hvcc_prefix);
    tcase_add_test(tc_4, test_merge_hevc_hvcc_suffix);
    tcase_add_test(tc_4, test_merge_hevc_hvcc_vclonly);
    tcase_add_test(tc_4, test_merge_hevc_hvcc_mix);
    tcase_add_test(tc_4, test_merge_hevc_hvcc_zero_pading);

    return s;
}

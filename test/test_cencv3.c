#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "libavformat/movenccencv3.h"

static const uint8_t key[16] = {0x53, 0x3a, 0x58, 0x3a,
                                0x84, 0x34, 0x36, 0xa5,
                                0x36, 0xfb, 0xe2, 0xa5,
                                0x82, 0x1c, 0x4b, 0x6c};
static const uint8_t iv_cenc[16] = {0x3a, 0xad, 0xb9, 0x91,
                                    0x20, 0xee, 0x24, 0x7d,
                                    0x0,  0x0,  0x0,  0x0,
                                    0x0,  0x0,  0x0,  0x0};

//for annexb content
static const uint8_t iv_cenc_annexb[16] = {0x7a, 0x4c, 0x7c, 0xb5,
                                           0x78, 0xe6, 0x07, 0x87,
                                           0x0,  0x0,  0x0,  0x0,
                                           0x0,  0x0,  0x0,  0x0};
void read_binary(char* file, uint8_t** ptr, size_t* size)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    size_t s = ftell(fp);
    *ptr = malloc(s + 10);//avoid invalid read try in valgrind
    *size = s;
    fseek(fp, 0, SEEK_SET);
    fread(*ptr, s, 1, fp);
    fclose(fp);
}

START_TEST(test_ff_mov_cenc_init)
{
    AVIOContext pb;
    AVFormatContext s;
    MOVMuxCencContext ctx;
    memset(&ctx, 0, sizeof(MOVMuxCencContext));

    int res = ff_mov_cencv3_init(&ctx, key, 1, 1);
    fail_unless(0 == res);
    ff_mov_cencv3_free(&ctx);
}
END_TEST

START_TEST(test_ff_mov_cencv3_avc_write_nal_units)
{
    uint8_t* extradata = NULL;
    size_t extradata_size = 0;
    uint8_t* sample1 = NULL;
    size_t sample1_size = 0;
    uint8_t* sample1_cenc = NULL;
    size_t sample1_cenc_size = 0;

    uint8_t* sample1_cenc_generated = NULL;

    read_binary(AVC_NON_ANNEXB_EXTRDADA_BIN_FILE, &extradata, &extradata_size);
    read_binary(AVC_NON_ANNEXB_SAMPLE1_BIN_FILE, &sample1, &sample1_size);
    read_binary(AVC_CENC_SAMPLE1_BIN_FILE, &sample1_cenc, &sample1_cenc_size);//generate by usp

    sample1_cenc_generated = malloc(sample1_cenc_size);
    AVIOContext pb;
    AVFormatContext s;
    MOVMuxCencContext ctx;
    memset(&ctx, 0, sizeof(MOVMuxCencContext));

    pb.buffer = sample1_cenc_generated;
    int res = ff_mov_cencv3_init(&ctx, key, 1, 1);
    fail_unless(0 == res);
    av_aes_ctr_set_iv(ctx.aes_ctr, iv_cenc);//manually set iv

    res = ff_mov_cencv3_parse_avc_XPS(&ctx, extradata, extradata_size);
    fail_unless(0 == res);

    res = ff_mov_cencv3_avc_write_nal_units(&s, &ctx, 4, &pb, sample1, sample1_size);
    fail_unless(0 == res);
    fail_unless(0 == memcmp(sample1_cenc_generated, sample1_cenc, sample1_cenc_size));

    ff_mov_cencv3_free(&ctx);
    free(extradata);
    free(sample1);
    free(sample1_cenc);
    free(sample1_cenc_generated);
}
END_TEST

START_TEST(test_ff_mov_cencv3_avc_parse_nal_units)
{
    uint8_t* extradata = NULL;
    size_t extradata_size = 0;
    uint8_t* sample1 = NULL;
    size_t sample1_size = 0;
    uint8_t* sample1_cenc = NULL;
    size_t sample1_cenc_size = 0;

    uint8_t* sample1_cenc_generated = NULL;

    read_binary(AVC_ANNEXB_EXTRDADA_BIN_FILE, &extradata, &extradata_size);
    read_binary(AVC_ANNEXB_SAMPLE1_BIN_FILE, &sample1, &sample1_size);
    read_binary(AVC_CENC_V3_SAMPLE1_FROM_ANNEXB_BIN_FILE, &sample1_cenc, &sample1_cenc_size);

    sample1_cenc_generated = malloc(sample1_cenc_size);
    AVIOContext pb;
    AVFormatContext s;
    MOVMuxCencContext ctx;
    memset(&ctx, 0, sizeof(MOVMuxCencContext));

    pb.buffer = sample1_cenc_generated;
    int res = ff_mov_cencv3_init(&ctx, key, 1, 1);
    fail_unless(0 == res);
    av_aes_ctr_set_iv(ctx.aes_ctr, iv_cenc_annexb);//manually set iv

    res = ff_mov_cencv3_parse_avc_XPS(&ctx, extradata, extradata_size);
    fail_unless(0 == res);

    res = ff_mov_cencv3_avc_parse_nal_units(&ctx, &pb, sample1, sample1_size);
    fail_unless(sample1_cenc_size == res);
    fail_unless(0 == memcmp(sample1_cenc_generated, sample1_cenc, sample1_cenc_size));

    ff_mov_cencv3_free(&ctx);
    free(extradata);
    free(sample1);
    free(sample1_cenc);
    free(sample1_cenc_generated);
}
END_TEST

Suite *test_suite(void)
{
    Suite *s = suite_create("Irdeto patch CENC cenc v3 test suite");
    TCase *tc = tcase_create("Irdeto patch CENC cenc v3 test cases");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_ff_mov_cenc_init);
    tcase_add_test(tc, test_ff_mov_cencv3_avc_write_nal_units);
    tcase_add_test(tc, test_ff_mov_cencv3_avc_parse_nal_units);
    return s;
}

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "libavformat/movenccbcs.h"

static const uint8_t key[16] = {0x53, 0x3a, 0x58, 0x3a,
                                0x84, 0x34, 0x36, 0xa5,
                                0x36, 0xfb, 0xe2, 0xa5,
                                0x82, 0x1c, 0x4b, 0x6c};
static const uint8_t iv_cbcs[16] = {0x9a, 0x68, 0x59, 0xa8,
                                    0x11, 0x04, 0x43, 0x8a,
                                    0x0,  0x0,  0x0,  0x0,
                                    0x0,  0x0,  0x0,  0x0};

//for annexb content
static const uint8_t iv_cbcs_annexb[16] = {0x42, 0x43, 0x18, 0x8f,
                                           0xf5, 0x6d, 0xa3, 0x1d,
                                           0xa6, 0x38, 0x62, 0xdc,
                                           0x7e, 0xa1, 0x0B, 0x84};
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

START_TEST(test_ff_mov_cbcs_init)
{
    AVIOContext pb;
    AVFormatContext s;
    MOVMuxCbcsContext ctx;
    memset(&ctx, 0, sizeof(MOVMuxCbcsContext));

    int res = ff_mov_cbcs_init(&ctx, key, iv_cbcs, 1, 9, 1);
    fail_unless(0 == res);
    ff_mov_cbcs_free(&ctx);
}
END_TEST

START_TEST(test_ff_mov_cbcs_avc_write_nal_units)
{
    uint8_t* extradata = NULL;
    size_t extradata_size = 0;
    uint8_t* sample1 = NULL;
    size_t sample1_size = 0;
    uint8_t* sample2 = NULL;
    size_t sample2_size = 0;
    uint8_t* sample1_cbcs = NULL;
    size_t sample1_cbcs_size = 0;
    uint8_t* sample2_cbcs = NULL;
    size_t sample2_cbcs_size = 0;

    uint8_t* sample1_cbcs_generated = NULL;
    uint8_t* sample2_cbcs_generated = NULL;

    read_binary(AVC_NON_ANNEXB_EXTRDADA_BIN_FILE, &extradata, &extradata_size);
    read_binary(AVC_NON_ANNEXB_SAMPLE1_BIN_FILE, &sample1, &sample1_size);
    read_binary(AVC_NON_ANNEXB_SAMPLE2_BIN_FILE, &sample2, &sample2_size);
    read_binary(AVC_CBCS_SAMPLE1_BIN_FILE, &sample1_cbcs, &sample1_cbcs_size);//generate by usp
    read_binary(AVC_CBCS_SAMPLE2_BIN_FILE, &sample2_cbcs, &sample2_cbcs_size);//generate by usp

    sample1_cbcs_generated = malloc(sample1_cbcs_size);
    sample2_cbcs_generated = malloc(sample2_cbcs_size);
    AVIOContext pb;
    AVFormatContext s;
    MOVMuxCbcsContext ctx;
    memset(&ctx, 0, sizeof(MOVMuxCbcsContext));

    pb.buffer = sample1_cbcs_generated;
    int res = ff_mov_cbcs_init(&ctx, key, iv_cbcs, 1, 9, 1);
    fail_unless(0 == res);

    res = ff_mov_avc_cbcs_parse_XPS(&ctx, extradata, extradata_size);
    fail_unless(0 == res);

    res = ff_mov_cbcs_avc_write_nal_units(&s, &ctx, 4, &pb, sample1, sample1_size);
    fail_unless(0 == res);
    fail_unless(0 == memcmp(sample1_cbcs_generated, sample1_cbcs, sample1_cbcs_size));

    pb.buffer = sample2_cbcs_generated;
    res = ff_mov_cbcs_avc_write_nal_units(&s, &ctx, 4, &pb, sample2, sample2_size);
    fail_unless(0 == res);
    fail_unless(0 == memcmp(sample2_cbcs_generated, sample2_cbcs, sample2_cbcs_size));

    ff_mov_cbcs_free(&ctx);
    free(extradata);
    free(sample1);
    free(sample2);
    free(sample1_cbcs);
    free(sample2_cbcs);
    free(sample1_cbcs_generated);
    free(sample2_cbcs_generated);
}
END_TEST

START_TEST(test_ff_mov_cbcs_avc_parse_nal_units)
{
    uint8_t* extradata = NULL;
    size_t extradata_size = 0;
    uint8_t* sample1 = NULL;
    size_t sample1_size = 0;
    uint8_t* sample2 = NULL;
    size_t sample2_size = 0;
    uint8_t* sample1_cbcs = NULL;
    size_t sample1_cbcs_size = 0;
    uint8_t* sample2_cbcs = NULL;
    size_t sample2_cbcs_size = 0;

    uint8_t* sample1_cbcs_generated = NULL;
    uint8_t* sample2_cbcs_generated = NULL;

    read_binary(AVC_ANNEXB_EXTRDADA_BIN_FILE, &extradata, &extradata_size);
    read_binary(AVC_ANNEXB_SAMPLE1_BIN_FILE, &sample1, &sample1_size);
    read_binary(AVC_ANNEXB_SAMPLE2_BIN_FILE, &sample2, &sample2_size);
    read_binary(AVC_CBCS_SAMPLE1_FROM_ANNEXB_BIN_FILE, &sample1_cbcs, &sample1_cbcs_size);
    read_binary(AVC_CBCS_SAMPLE2_FROM_ANNEXB_BIN_FILE, &sample2_cbcs, &sample2_cbcs_size);

    sample1_cbcs_generated = malloc(sample1_cbcs_size + 10);
    sample2_cbcs_generated = malloc(sample2_cbcs_size + 10);
    AVIOContext pb;
    AVFormatContext s;
    MOVMuxCbcsContext ctx;
    memset(&ctx, 0, sizeof(MOVMuxCbcsContext));

    pb.buffer = sample1_cbcs_generated;
    int res = ff_mov_cbcs_init(&ctx, key, iv_cbcs_annexb, 1, 9, 1);
    fail_unless(0 == res);

    res = ff_mov_avc_cbcs_parse_XPS(&ctx, extradata, extradata_size);
    fail_unless(0 == res);

    res = ff_mov_cbcs_avc_parse_nal_units(&ctx, &pb, sample1, sample1_size);
    fail_unless(sample1_cbcs_size == res);
    fail_unless(0 == memcmp(sample1_cbcs_generated, sample1_cbcs, sample1_cbcs_size));

    pb.buffer = sample2_cbcs_generated;
    res = ff_mov_cbcs_avc_parse_nal_units(&ctx, &pb, sample2, sample2_size);
    fail_unless(sample2_cbcs_size == res);
    fail_unless(0 == memcmp(sample2_cbcs_generated, sample2_cbcs, sample2_cbcs_size));

    ff_mov_cbcs_free(&ctx);
    free(extradata);
    free(sample1);
    free(sample2);
    free(sample1_cbcs);
    free(sample2_cbcs);
    free(sample1_cbcs_generated);
    free(sample2_cbcs_generated);
}
END_TEST

Suite *test_suite(void)
{
    Suite *s = suite_create("Irdeto patch CENC cbcs test suite");
    TCase *tc = tcase_create("Irdeto patch CENC cbcs test cases");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_ff_mov_cbcs_init);
    tcase_add_test(tc, test_ff_mov_cbcs_avc_write_nal_units);
    tcase_add_test(tc, test_ff_mov_cbcs_avc_parse_nal_units);
    return s;
}

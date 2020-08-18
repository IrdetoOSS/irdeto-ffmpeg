#include <stdio.h>
#include <check.h>
#include <stdint.h>
#include <sys/time.h>
#include "libavutil/time.h"
#include "test_mock.h"
#include "libavformat/avc.h"
/*
************************************************************************************************************************
* start of pre-defined struct and func
************************************************************************************************************************
*/

#define MAX_PPS_NUM 255 // 0 - 255
#define MAX_SPS_NUM 31 // 0 - 31
/**
* @brief    Structure of AVC sps info, prepared for the parsing of the vcl slice header, so it
*           only has partial sps info
*/
typedef struct
{
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
typedef struct
{
    avc_sps_t* sps;
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
typedef struct
{
    avc_pps_t pps_list[MAX_PPS_NUM];
    avc_sps_t sps_list[MAX_SPS_NUM];
} avc_extra_data_ctx_t;


/*
************************************************************************************************************************
* end of pre-defined struct and func
************************************************************************************************************************
*/


static const uint8_t sps_buf1[24] = {0x67, 0x4d, 0x40, 0x29,
                                     0x96, 0x52, 0x80, 0xf0,
                                     0x04, 0x4f, 0xcb, 0x35,
                                     0x01, 0x01, 0x01, 0x40,
                                     0x00, 0x00, 0xfa, 0x40,
                                     0x00, 0x2e, 0xe0, 0x21};

static const uint8_t pps_buf1[5] = {0x68, 0xe9, 0x09, 0x35, 0x20};
static const uint8_t slice_buf1[150] = {0x65, 0x88, 0x80, 0x40, 0x02, 0xdb, 0xaa, 0xe2,
                                     0xa9, 0xf5, 0xce, 0x9a, 0x4c, 0x97, 0x3e, 0x17,
                                     0x3d, 0xbc, 0x5a, 0xdd, 0xc4, 0xf4, 0xcd, 0x7b,
                                     0x47, 0x8c, 0xa1, 0x0e, 0x7d, 0x8b, 0x36, 0xd8,
                                     0xee, 0x7b, 0x42, 0xe6, 0xc9, 0xd1, 0x60, 0x5d,
                                     0xa6, 0x45, 0x4b, 0x52, 0xff, 0x76, 0xfb, 0x77,
                                     0x77, 0x5f, 0x07, 0x3a, 0x90, 0xbf, 0x58, 0x55,
                                     0xc8, 0x88, 0x4b, 0x58, 0xf9, 0xb9, 0xda, 0xaa,
                                     0x32, 0x1c, 0x5d, 0x9c, 0xe7, 0xa1, 0x27, 0xa0,
                                     0xb8, 0x70, 0xbe, 0xc4, 0x35, 0x1e, 0xe9, 0x21,
                                     0xef, 0x11, 0xee, 0x81, 0x22, 0x0c, 0xb3, 0x62,
                                     0xcf, 0xf9, 0x36, 0x5c, 0x94, 0x8b, 0xe8, 0x38,
                                     0x50, 0x0a, 0xdb, 0x64, 0x86, 0x1d, 0x09, 0x64,
                                     0x97, 0xe9, 0xca, 0xdb, 0x57, 0xa4, 0xd0, 0x99,
                                     0x47, 0xa1, 0x9f, 0xd5, 0xe0, 0xca, 0xf4, 0x22,
                                     0x58, 0x80, 0x35, 0xd4, 0xfb, 0xf9, 0x07, 0x39,
                                     0x68, 0x0c, 0x68, 0xad, 0xaa, 0x16, 0xad, 0x30,
                                     0x27, 0xa5, 0x8e, 0x7b, 0xe8, 0x4c, 0xcc, 0x7c,
                                     0xdd, 0x3c, 0x37, 0xdf, 0x5d, 0xb0};
static const size_t c_slice_buf1_header = 6;//nal header + slice header

static const uint8_t slice_buf1_2[58] = {0x41, 0x9a, 0x02, 0x05, 0xb1, 0x0f, 0x5b, 0xfe,
                                         0xba, 0x45, 0xf1, 0xd6, 0x8b, 0x73, 0xaa, 0x0d,
                                         0xf7, 0x8c, 0xd4, 0x08, 0xca, 0xf4, 0xed, 0xdf,
                                         0xda, 0xd2, 0xe0, 0xc8, 0x90, 0x8a, 0x8b, 0x2a,
                                         0xd5, 0x28, 0x9b, 0xa6, 0x16, 0xa3, 0x8a, 0x26,
                                         0x77, 0x4d, 0xcd, 0x9f, 0xd5, 0xf5, 0x07, 0x12,
                                         0x9e, 0xaf, 0xaf, 0xb0, 0xec, 0xfb, 0xfe, 0xd1,
                                         0xff, 0x1a};
static const size_t c_slice_buf1_2_header = 7;//nal header + slice header

static const uint8_t sps_buf2[24] = {0x67, 0x64, 0x00, 0x1f,
                                     0xac, 0xd9, 0x40, 0x50,
                                     0x05, 0xbb, 0x01, 0x10,
                                     0x00, 0x00, 0x3e, 0x90,
                                     0x00, 0x0b, 0xb8, 0x00,
                                     0xf1, 0x83, 0x19, 0x60};

static const uint8_t pps_buf2[6] = {0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0};

static const uint8_t slice_buf2[211] = {0x65, 0x88, 0x84, 0x00, 0x2f, 0xff, 0xfe, 0xf6,
                                        0xae, 0xfc, 0xcb, 0x2b, 0x74, 0x7e, 0x95, 0x2e,
                                        0x1d, 0x59, 0x7b, 0xb3, 0x51, 0xf2, 0xe8, 0x49,
                                        0x72, 0xfd, 0x88, 0x30, 0x7d, 0x68, 0x00, 0x00,
                                        0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0xa8, 0x33, 0x30, 0xc8, 0xce,
                                        0xf5, 0xdb, 0x36, 0xdd, 0x40, 0x00, 0x00, 0x03,
                                        0x00, 0x00, 0x1f, 0x00, 0x00, 0x1d, 0x40, 0x00,
                                        0x37, 0x40, 0x00, 0x99, 0x80, 0x01, 0xec, 0x00,
                                        0x06, 0x48, 0x00, 0x1c, 0x40, 0x00, 0x7f, 0x00,
                                        0x02, 0x64, 0x00, 0x10, 0xd0, 0x00, 0x7d, 0x80,
                                        0x03, 0x14, 0x00, 0x1d, 0x60, 0x00, 0x00, 0x03,
                                        0x00, 0x00, 0x03, 0x01, 0x51, 0x20, 0x28, 0xa0,
                                        0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
                                        0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                        0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
                                        0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                        0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
                                        0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                        0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
                                        0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
                                        0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
                                        0x03, 0x01, 0x01};
static const size_t c_slice_buf2_header = 6;//nal header + slice header
START_TEST(test_avc_parser_parse_sps)
{
    avc_sps_t sps1;
    int res = avc_parser_parse_sps(sps_buf1, 24, &sps1);
    fail_unless(0 == res);
    fail_unless(0 == sps1.seq_parameter_set_id);
    fail_unless(68 == sps1.pic_height_in_map_units);
    fail_unless(120 == sps1.pic_width_in_mbs);
    fail_unless(8 == sps1.log2_max_frame_num);
    fail_unless(0 == sps1.pic_order_cnt_type);
    fail_unless(8 == sps1.log2_max_pic_order_cnt_lsb);
    fail_unless(1 == sps1.frame_mbs_only_flag);

    avc_sps_t sps2;
    res = avc_parser_parse_sps(sps_buf2, 24, &sps2);
    fail_unless(0 == res);
    fail_unless(0 == sps2.seq_parameter_set_id);
    fail_unless(1 == sps2.chroma_format_idc);
    fail_unless(4 == sps2.log2_max_frame_num);
    fail_unless(0 == sps2.pic_order_cnt_type);
    fail_unless(6 == sps2.log2_max_pic_order_cnt_lsb);
    fail_unless(45 == sps2.pic_height_in_map_units);
    fail_unless(80 == sps2.pic_width_in_mbs);
    fail_unless(1 == sps2.frame_mbs_only_flag);
}
END_TEST

START_TEST(test_avc_parser_parse_pps)
{
    avc_pps_t pps;
    int res = avc_parser_parse_pps(pps_buf1, 5, &pps);
    fail_unless(0 == res);
    fail_unless(0 == pps.pic_parameter_set_id);
    fail_unless(0 == pps.seq_parameter_set_id);
    fail_unless(1 == pps.entropy_coding_mode_flag);
    fail_unless(0 == pps.bottom_field_pic_order_in_frame_present_flag);
    fail_unless(4 == pps.num_ref_idx[0]);
    fail_unless(4 == pps.num_ref_idx[1]);
    fail_unless(0 == pps.num_slice_groups_minus1);
    fail_unless(0 == pps.weighted_bipred_idc);
    fail_unless(1 == pps.weighted_pred_flag);
    fail_unless(1 == pps.deblocking_filter_control_present_flag);
    fail_unless(0 == pps.redundant_pic_cnt_present_flag);

    avc_pps_t pps2;
    res = avc_parser_parse_pps(pps_buf2, 5, &pps2);
    fail_unless(0 == res);
    fail_unless(0 == pps2.pic_parameter_set_id);
    fail_unless(0 == pps2.seq_parameter_set_id);
    fail_unless(1 == pps2.entropy_coding_mode_flag);
    fail_unless(0 == pps2.bottom_field_pic_order_in_frame_present_flag);
    fail_unless(3 == pps2.num_ref_idx[0]);
    fail_unless(1 == pps2.num_ref_idx[1]);
    fail_unless(0 == pps2.num_slice_groups_minus1);
    fail_unless(1 == pps2.weighted_pred_flag);
    fail_unless(2 == pps2.weighted_bipred_idc);
    fail_unless(1 == pps2.deblocking_filter_control_present_flag);
    fail_unless(0 == pps2.redundant_pic_cnt_present_flag);
}
END_TEST

START_TEST(test_avc_parser_parse_slice_header)
{
    avc_sps_t sps[MAX_SPS_NUM];
    avc_pps_t pps[MAX_PPS_NUM];

    memset(&sps[0], 0, sizeof(avc_sps_t));
    int res = avc_parser_parse_sps(sps_buf1, 24, &sps[0]);//the id is 0, so use 0th element
    fail_unless(0 == res);
    memset(&pps[0], 0, sizeof(avc_pps_t));
    res = avc_parser_parse_pps(pps_buf1, 5, &pps[0]);//the id is 0, so use 0th element
    fail_unless(0 == res);
    pps[0].sps = &sps[pps[0].seq_parameter_set_id];
    size_t slice_header_size = 0;
    res = ff_avc_parser_get_slice_header_size(pps, slice_buf1, 211, &slice_header_size);
    fail_unless(0 == res);
    fail_unless(c_slice_buf1_header== slice_header_size);

    res = ff_avc_parser_get_slice_header_size(pps, slice_buf1_2, 58, &slice_header_size);
    fail_unless(0 == res);
    fail_unless(c_slice_buf1_2_header== slice_header_size);



    memset(&sps[0], 0, sizeof(avc_sps_t));
    res = avc_parser_parse_sps(sps_buf2, 24, &sps[0]);//the id is 0, so use 0th element
    fail_unless(0 == res);
    memset(&pps[0], 0, sizeof(avc_pps_t));
    res = avc_parser_parse_pps(pps_buf2, 5, &pps[0]);//the id is 0, so use 0th element
    fail_unless(0 == res);
    pps[0].sps = &sps[pps[0].seq_parameter_set_id];
    slice_header_size = 0;
    res = ff_avc_parser_get_slice_header_size(pps, slice_buf2, 211, &slice_header_size);
    fail_unless(0 == res);
    fail_unless(c_slice_buf2_header== slice_header_size);
}
END_TEST



Suite *test_suite(void)
{
    Suite *s = suite_create("Irdeto avc parser test suite");
    TCase *tc = tcase_create("Irdeto avc parser test cases");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_avc_parser_parse_sps);
    tcase_add_test(tc, test_avc_parser_parse_pps);
    tcase_add_test(tc, test_avc_parser_parse_slice_header);
    return s;
}

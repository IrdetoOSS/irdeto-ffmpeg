/**
********************************************************************************
* @file         ir_xps_common.h
* @brief        Common definition shared by export and import interface
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Mar 09, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/

#ifndef _IR_XPS_COMMON_H_
#define _IR_XPS_COMMON_H_

#include <stdint.h>
#include <stddef.h>

#define  IRXPS_NUM_REFS   2
#define  IRXPS_NUM_PLANES 3

/**
********************************************************************************
* @brief        Return status of the interface
********************************************************************************
*/
typedef enum
{
    IR_XPS_EXPORT_STATUS_OK     = 0,        ///< Success
    IR_XPS_EXPORT_STATUS_NI     = 1,        ///< Not implemented yet
    IR_XPS_EXPORT_STATUS_BADARG = 2,        ///< Bad arguments
    IR_XPS_EXPORT_STATUS_BADPRO = 3,        ///< Profile not supported
    IR_XPS_EXPORT_STATUS_FAIL   = -1        ///< Failed

} IR_XPS_EXPORT_STATUS;

typedef struct ir_xps_export_hevc
{
    struct hevc_sps
    {
        uint32_t    seq_parameter_set_id;
        uint32_t    general_level_idc;
        uint32_t    pic_width_in_luma_samples;
        uint32_t    pic_height_in_luma_samples;
        uint32_t    bit_depth_luma_minus8;
        uint32_t    log2_max_pic_order_count_lsb_minus4;
        uint32_t    log2_min_luma_coding_block_size_minus3;
        uint32_t    log2_diff_max_min_luma_coding_block_size;
        uint32_t    log2_min_luma_transform_block_size_minus2;
        uint32_t    log2_diff_max_min_luma_transform_block_size;
        uint32_t    max_transform_hierarchy_depth_inter;
        uint32_t    max_transform_hierarchy_depth_intra;
        uint32_t    scaling_list_enabled_flag;
        uint32_t    sps_scaling_list_data_present_flag;
        uint32_t    amp_enable_flag;
        uint32_t    sample_adaptive_offset_enabled_flag;
        uint32_t    num_short_term_ref_pic_sets;
        uint32_t    sps_temporal_mvp_enabled_flag;
        uint32_t    strong_intra_smoothing_enabled_flag;
    } sps;

    struct hevc_pps
    {
        uint32_t    output_flag_present_flag;
        uint32_t    tiles_enabled_flag;
        uint32_t    entropy_coding_sync_enabled;
        uint32_t    pps_scaling_list_data_present_flag;
    } pps;

    struct hevc_slice_header
    {
        uint32_t    slice_type;
        uint32_t    slice_picture_order_cnt_lsb;
    } slh;

    struct hevc_parsed
    {
        uint32_t is_ref;
        uint32_t is_output;
        uint32_t nal_unit_type;
        uint32_t active_scaling_list[256];
    } parsed;

    struct hevc_rps
    {
        uint32_t num_negative_pics;
        uint32_t num_positive_pics;
        int32_t deltaPOCs[16];
    } rps;

	void *pkt; // avpacket form source stream assumed to contain full AU

} ir_xps_export_hevc;

typedef struct ir_ref
{
    uint32_t    linesize[IRXPS_NUM_PLANES];
    uint32_t    height;
    int32_t     poc;        ///< Only used for AVC
    uint8_t*    data[IRXPS_NUM_PLANES];

    /**
    ****************************************************************************
    * @note     Placeholder for reference frames, this member cannot be accessed
    *           outside, introduced to resolve multithread decoding issues
    * @note     It will be pointed in correct place during exporting meta data,
    *           however, because of multithread decoding, data inside reference
    *           frames are not ready yet at that moment
    ****************************************************************************
    */
    void*       avframe;
} ir_ref;

typedef struct ir_xps_export_avc
{
    struct avc_sps
    {
        uint32_t    profile_idc;
        uint32_t    level_idc;
        uint32_t    seq_parameter_set_id;
        uint32_t    log2_max_frame_num_minus4;
        uint32_t    pic_order_cnt_type;
        uint32_t    log2_max_pic_order_cnt_lsb_minus4;
        uint32_t    max_num_ref_frames;
        uint32_t    frame_mbs_only_flag;
    } sps;

    struct avc_slice_header
    {
        uint32_t    frame_num;
    } slice_header;

    struct avc_parsed
    {
        int32_t     poc;
    } parsed;

	void *pkt; // avpacket form source stream assumed to contain full AU

} ir_xps_export_avc;

typedef struct ir_xps_export_mpeg2
{
	/**
     ************************************************************************
     * @note    Data extracted from sequence header, sequence extension and
     *  		picture header
     ************************************************************************
     */
	uint32_t width;
	uint32_t height;
	uint32_t progressive_sequence;
	uint32_t chroma_format;
	uint32_t picture_coding_type;

	/**
     ************************************************************************
     * @note    Quantization matrix values from sequence header and
     * 			quantization matrix extension sequence extension and picture
     * 			header
     ************************************************************************
     */
	uint16_t intra_quantiser_matrix[64];
	uint16_t non_intra_quantiser_matrix[64];
	uint16_t chroma_intra_quantiser_matrix[64];
	uint16_t chroma_non_intra_quantiser_matrix[64];

	/**
     ************************************************************************
     * @note     Picture Coding Extension (full except composite display flag)
     ************************************************************************
     */
	uint32_t f_code00;
	uint32_t f_code01;
	uint32_t f_code10;
	uint32_t f_code11;
	uint32_t intra_dc_precision;
	uint32_t picture_structure;
	uint32_t top_field_first;
	uint32_t frame_pred_frame_dct;
	uint32_t concealment_motion_vectors;
	uint32_t q_scale_type;
	uint32_t intra_vlc_format;
	uint32_t alternate_scan;
	uint32_t repeat_first_field;
	uint32_t chroma_420_type;
	uint32_t progressive_frame;

	// picture guid used by encoder to calc temporal distance for B frames.
	uint64_t guid;

	uint32_t mb_width;
	uint32_t mb_height;
	uint32_t mb_stride;
	uint8_t *qscale_values;
	void *pkt; // avpacket form source stream assumed to contain full AU
	uint32_t is_imx;


} ir_xps_export_mpeg2;

/**
********************************************************************************
* @enum         IR_CONTEXT_STATE
* @brief        Internal context state
********************************************************************************
*/
typedef enum
{
    IR_CONTEXT_STATE_UNINITIALIZED  = 0,    ///< Context not valid
    IR_CONTEXT_STATE_INITIALIZED    = 1,    ///< Context valid
    IR_CONTEXT_STATE_READY          = 2     ///< Ready to be imported

} IR_CONTEXT_STATE;

struct ir_header
{
    size_t              size;
    IR_CONTEXT_STATE    state;
};

typedef struct
{
    struct ir_header header;

    ir_xps_export_hevc hevc_meta;
    ir_xps_export_avc  avc_meta;
    ir_xps_export_mpeg2 mpeg2_meta;

    ir_ref ref_frame[IRXPS_NUM_REFS];

    uint32_t is_ref;

} ir_xps_context;

ir_xps_context* ir_xps_context_create();

IR_XPS_EXPORT_STATUS ir_xps_vc(const ir_xps_context* const xps_context);

void ir_xps_context_destroy(ir_xps_context* xps_context);

#endif  /* _IR_XPS_COMMON_H_ */

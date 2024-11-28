/**
********************************************************************************
* @file         ir_xps_export_hevc.h
* @brief        Irdeto unified interface between decoder and encoder
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Mar 09, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/

#ifndef _IR_XPS_EXPORT_HEVC_H_
#define _IR_XPS_EXPORT_HEVC_H_

#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/common.h>

#include "ir_xps_common.h"

struct HEVCContext;
struct HEVCFrame;
int ff_hevc_frame_nb_refs(struct HEVCContext* s);
struct HEVCFrame* find_ref_idx(struct HEVCContext* s, int poc);

IR_XPS_EXPORT_STATUS ir_xps_export_x265(const struct HEVCContext* const s,
    ir_xps_context* xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == s)
        {
            break;
        }

        if (NULL == xps_context)
        {
            break;
        }

        ir_xps_export_hevc* xps = &(xps_context->hevc_meta);

        /**
        ************************************************************************
        * @note     Export SPS data
        ************************************************************************
        */
        xps->sps.seq_parameter_set_id = s->ps.pps->sps_id;
        xps->sps.general_level_idc = s->ps.vps->ptl.general_ptl.level_idc;
        xps->sps.pic_width_in_luma_samples = s->ps.sps->width;
        xps->sps.pic_height_in_luma_samples = s->ps.sps->height;
        xps->sps.bit_depth_luma_minus8 = s->ps.sps->bit_depth - 8;
        xps->sps.log2_max_pic_order_count_lsb_minus4
            = s->ps.sps->log2_max_poc_lsb - 4;
        xps->sps.log2_min_luma_coding_block_size_minus3
            = s->ps.sps->log2_min_cb_size - 3;
        xps->sps.log2_diff_max_min_luma_coding_block_size
            = s->ps.sps->log2_diff_max_min_coding_block_size;
        xps->sps.log2_min_luma_transform_block_size_minus2
            = s->ps.sps->log2_min_tb_size - 2;
        xps->sps.log2_diff_max_min_luma_transform_block_size
            = s->ps.sps->log2_max_trafo_size - s->ps.sps->log2_min_tb_size;
        xps->sps.max_transform_hierarchy_depth_inter
            = s->ps.sps->max_transform_hierarchy_depth_inter;
        xps->sps.max_transform_hierarchy_depth_intra
            = s->ps.sps->max_transform_hierarchy_depth_intra;
        xps->sps.scaling_list_enabled_flag
            = s->ps.sps->scaling_list_enable_flag;
        xps->sps.sps_scaling_list_data_present_flag
            = s->ps.sps->sps_scaling_list_data_present_flag;
        xps->sps.amp_enable_flag = s->ps.sps->amp_enabled_flag;
        xps->sps.sample_adaptive_offset_enabled_flag = s->ps.sps->sao_enabled;
        xps->sps.num_short_term_ref_pic_sets = s->ps.sps->nb_st_rps;
        xps->sps.sps_temporal_mvp_enabled_flag =
            s->ps.sps->sps_temporal_mvp_enabled_flag;
        xps->sps.strong_intra_smoothing_enabled_flag
            = s->ps.sps->sps_strong_intra_smoothing_enable_flag;

        /**
        ************************************************************************
        * @note     Export PPS data
        ************************************************************************
        */
        xps->pps.output_flag_present_flag = s->ps.pps->output_flag_present_flag;
        xps->pps.tiles_enabled_flag = s->ps.pps->tiles_enabled_flag;
        xps->pps.entropy_coding_sync_enabled
            = s->ps.pps->entropy_coding_sync_enabled_flag;
        xps->pps.pps_scaling_list_data_present_flag
            = s->ps.pps->scaling_list_data_present_flag;

        /**
        ************************************************************************
        * @note     Export slice header data
        ************************************************************************
        */
        xps->slh.slice_type = s->sh.slice_type;
        xps->slh.slice_picture_order_cnt_lsb = s->sh.pic_order_cnt_lsb;
        xps->slh.pps_id = s->sh.pps_id;
        xps->parsed.nal_unit_type = s->first_nal_type;
        xps->parsed.is_output = 1;
        if(xps->pps.output_flag_present_flag)
        {
            xps->parsed.is_output = s->sh.pic_output_flag;
        }

        /**
        ************************************************************************
        * @note     Export RPS data
        ************************************************************************
        */
        uint32_t i = 0, MAX_NUM_REF = 16;
        memset(xps->rps.deltaPOCs, 0, sizeof(xps->rps.deltaPOCs));
        if (s->sh.short_term_rps != NULL)
        {
            xps->rps.num_negative_pics =
                s->sh.short_term_rps->num_negative_pics;
            xps->rps.num_positive_pics =
                s->sh.short_term_rps->num_delta_pocs -
                s->sh.short_term_rps->num_negative_pics;
            for (i = 0; i < s->sh.short_term_rps->num_delta_pocs; i++)
            {
                xps->rps.deltaPOCs[i] = s->sh.short_term_rps->delta_poc[i];
            }
        }
    
        /**
        ************************************************************************
        * @note     Export reference frame
        ************************************************************************
        */
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(s->ref->frame->format);
        
        const HEVCWindow* window = &s->ps.sps->output_window;
        
        int pixel_shift = !!(desc->comp[0].depth > 8);
        int k = 0;
        for (k = 0; k < 2; k++)
        {
            AVFrame* dst = NULL;
            if (s->ref->refPicList && s->ref->refPicList[k].nb_refs)
            {
                struct HEVCFrame *hevc_ref = find_ref_idx(s,
                    s->ref->refPicList[k].ref[0]->poc);
                if (hevc_ref != NULL &&
                    (dst = av_frame_clone(hevc_ref->frame)) != NULL)
                {
                    xps_context->ref_frame[k].height = hevc_ref->frame->height;
                    for (i = 0; i < 3; i++)
                    {
                        int hshift = (i > 0) ? desc->log2_chroma_w : 0;
                        int vshift = (i > 0) ? desc->log2_chroma_h : 0;
                        int off = ((window->left_offset >> hshift) <<
                            pixel_shift) + (window->top_offset >>
                            vshift) * dst->linesize[i];
                        dst->data[i] += off;
                        xps_context->ref_frame[k].linesize[i] = dst->linesize[i];
                    }
                    xps_context->ref_frame[k].avframe = dst;
                }
            }
        }

        /**
          ************************************************************************
          * @note 	Export source packet to be decoded
          ************************************************************************
        */
        xps_context->hevc_meta.pkt = av_packet_clone(s->pkt_strm);

        xps_context->header.state = IR_CONTEXT_STATE_READY;

        /**
        ************************************************************************
        * @note     Export scaling matrix if scaling_list_enable_flag present
        * @note     size0: (delta_coef x 16) x 6
        * @note     size1: (delta_coef x 64) x 6
        * @note     size2: (delta_coef x 64) x 6
        * @note     size2: (delta_coef x 64) x 2
        * @note     size2: dc_coef x 6
        * @note     size3: dc_coef x 2
        ************************************************************************
        */
        uint8_t* sm_ptr = (uint8_t*)xps->parsed.active_scaling_list;
        memset(sm_ptr, 0, sizeof(xps->parsed.active_scaling_list));
        uint32_t sizeId, matrixId, coefNum = 16, buffCntr = 0;
        ScalingList* sl = NULL;

        if(s->ps.pps->scaling_list_data_present_flag)
        {
            sl = &s->ps.pps->scaling_list;
        }
        else if(s->ps.sps->scaling_list_enable_flag &&
            s->ps.sps->sps_scaling_list_data_present_flag)
        {
            sl = &s->ps.sps->scaling_list;
        }
        else
        {
            result = IR_XPS_EXPORT_STATUS_OK;
            break;
        }

        coefNum = 16;
        for(sizeId = 0; sizeId < 4; sizeId++)
        {
            for(matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1)
            {
                memcpy(sm_ptr + buffCntr, &sl->sl[sizeId][matrixId][0],
                    coefNum);
                buffCntr += coefNum;
            }
            coefNum = 64;
        }

        memcpy(sm_ptr + buffCntr, sl->sl_dc[0], 6);
        buffCntr += 6;
        sm_ptr[buffCntr++] = sl->sl_dc[1][0];
        sm_ptr[buffCntr] = sl->sl_dc[1][3];

        result = IR_XPS_EXPORT_STATUS_OK;
    } while(0);

    return result;
}

#endif  /* !_IR_XPS_EXPORT_HEVC_H_ */

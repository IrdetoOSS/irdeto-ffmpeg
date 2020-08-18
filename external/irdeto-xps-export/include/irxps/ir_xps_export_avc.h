/**
********************************************************************************
* @file         ir_xps_export_avc.h
* @brief        Irdeto unified interface between decoder and encoder for AVC
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Apr 03, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/

#ifndef _IR_XPS_EXPORT_AVC_H_
#define _IR_XPS_EXPORT_AVC_H_

#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/common.h>

#include "ir_xps_common.h"

/**
********************************************************************************
* @note         Forward declaration of AVC related structure and function
********************************************************************************
*/
struct H264Context;
struct H264Picture;

IR_XPS_EXPORT_STATUS ir_xps_export_x264(const struct H264Context* const h,
    ir_xps_context* xps_context, int ref_idc)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == h)
        {
            break;
        }

        if (NULL == xps_context)
        {
            break;
        }

        xps_context->is_ref |= ref_idc;

        if (h->current_slice == 1)
        {
            /**
            ********************************************************************
            * @note     Export SPS data
            ********************************************************************
            */
            xps_context->avc_meta.sps.profile_idc = h->ps.sps->profile_idc;
            xps_context->avc_meta.sps.level_idc = h->ps.sps->level_idc;
            xps_context->avc_meta.sps.seq_parameter_set_id = h->ps.sps->sps_id;
            xps_context->avc_meta.sps.log2_max_frame_num_minus4 =
                h->ps.sps->log2_max_frame_num - 4;
            xps_context->avc_meta.sps.pic_order_cnt_type = h->ps.sps->poc_type;
            xps_context->avc_meta.sps.log2_max_pic_order_cnt_lsb_minus4 =
                h->ps.sps->log2_max_poc_lsb - 4;
            xps_context->avc_meta.sps.max_num_ref_frames = h->ps.sps->ref_frame_count;
            xps_context->avc_meta.sps.frame_mbs_only_flag = h->ps.sps->frame_mbs_only_flag;

            /**
            ********************************************************************
            * @note     Export slice header data
            ********************************************************************
            */
            xps_context->avc_meta.slice_header.frame_num = h->cur_pic_ptr->frame_num;

            /**
            ********************************************************************
            * @note     Export parsed data
            ********************************************************************
            */
            xps_context->is_ref = h->nal_ref_idc;
            xps_context->avc_meta.parsed.poc = h->cur_pic_ptr->poc - 65536;

            /**
            ********************************************************************
            * @note     Export reference frames
            ********************************************************************
            */
            H264Picture *pict_l0 = NULL, *pict_l1 = NULL;
            for (int frm_cntr = 0; frm_cntr < h->short_ref_count; frm_cntr++)
            {
                if (h->short_ref[frm_cntr]->poc < h->cur_pic_ptr->poc &&
                   (pict_l0 == NULL || h->short_ref[frm_cntr]->poc > pict_l0->poc))
                {
                    pict_l0 = h->short_ref[frm_cntr];
                }
                else if (h->short_ref[frm_cntr]->poc > h->cur_pic_ptr->poc)
                {
                    if (pict_l1 == NULL || h->short_ref[frm_cntr]->poc < pict_l1->poc)
                    {
                        pict_l1 = h->short_ref[frm_cntr];
                    }
                }
            }

            if (NULL == pict_l0)
            {
                pict_l0 = pict_l1;
            }
            if (NULL == pict_l1)
            {
                pict_l1 = pict_l0;
            }

            const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(
                h->cur_pic_ptr->f->format);
            for (int k = 0; k < 2; k++)
            {
                H264Picture* src = (k == 0) ? pict_l0 : pict_l1;
                AVFrame* dst = NULL;
                if (src != NULL && (dst = av_frame_clone(src->f)) != NULL)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        int hshift = (i > 0) ? desc->log2_chroma_w : 0;
                        int vshift = (i > 0) ? desc->log2_chroma_h : 0;
                        int off    = ((h->crop_left >> hshift) << h->pixel_shift) +
                                     (h->crop_top  >> vshift) * dst->linesize[i];
                        dst->data[i] += off;
                        xps_context->ref_frame[k].linesize[i] = dst->linesize[i];
                    }
                    xps_context->ref_frame[k].poc = src->poc - 65536;
                    xps_context->ref_frame[k].height = dst->height;
                }
                xps_context->ref_frame[k].avframe = dst;
            }
        }

        /**
          ************************************************************************
          * @note 	Export source packet to be decoded
          ************************************************************************
        */
        xps_context->avc_meta.pkt = av_packet_clone(h->pkt_strm);

        xps_context->header.state = IR_CONTEXT_STATE_READY;
        result = IR_XPS_EXPORT_STATUS_OK;
    } while(0);

    return result;
}

#endif /*_IR_XPS_EXPORT_AVC_H_*/

/**
********************************************************************************
* @file         ir_xps_import_avc.h
* @brief        Irdeto unified interface between decoder and encoder
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Apr 05, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/

#ifndef _IR_XPS_IMPORT_AVC_H_
#define _IR_XPS_IMPORT_AVC_H_

#include <stdint.h>
#include <x264.h>

#include "ir_xps_common.h"

IR_XPS_EXPORT_STATUS ir_xps_import_meta_x264(x264_t* const h,
    const ir_xps_context* const xps_context)
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

        if (xps_context->header.state != IR_CONTEXT_STATE_READY)
        {
            break;
        }

        const ir_xps_export_avc* xps = &(xps_context->avc_meta);

        if (100 == xps->sps.profile_idc)
        {
            x264_param_apply_profile(&h->param, "high");
        }
        else if (77 == xps->sps.profile_idc)
        {
            x264_param_apply_profile(&h->param, "main");
        }
        else
        {
            result = IR_XPS_EXPORT_STATUS_BADPRO;
            break;
        }

        /**
        ************************************************************************
        * @note     Import SPS parameters, originally set inside
        *           x264_sps_init, called during x264_encoder_open
        ************************************************************************
        */
        h->param.i_sps_id = xps->sps.seq_parameter_set_id;
        h->param.i_level_idc = xps->sps.level_idc;

        h->sps[0].i_profile_idc = xps->sps.profile_idc;


        h->sps[0].i_num_ref_frames = xps->sps.max_num_ref_frames;
        h->sps[0].i_log2_max_frame_num = xps->sps.log2_max_frame_num_minus4 + 4;
        h->sps[0].i_poc_type = xps->sps.pic_order_cnt_type;
        if (0 == xps->sps.pic_order_cnt_type)
        {
            h->sps[0].i_log2_max_poc_lsb =
                xps->sps.log2_max_pic_order_cnt_lsb_minus4 + 4;
        }
        h->sps[0].b_frame_mbs_only = xps->sps.frame_mbs_only_flag;

        /**
        ************************************************************************
        * @note     Import slice header parameters, originally set inside
        *           x264_slices_write, called in the end of x264_encoder_encode
        * @note     This two values are going to be overwriten by
        *           x264_slices_init
        ************************************************************************
        */
        // h->sh.i_frame_num = xps->slice_header.frame_num;
        // h->sh.i_poc = xps->parsed.poc;

        result = IR_XPS_EXPORT_STATUS_OK;
    } while(0);

    return result;
}

static void set_ref_planes(x264_picture_t* pic, ir_ref ref_frame)
{
    x264_picture_init(pic);
    pic->img.i_csp = X264_CSP_I420;
    int i = 0;

    for (i = 0; i < 3; i++)
    {
        if (ref_frame.data[i] != NULL)
        {
            pic->img.plane[i] = ref_frame.data[i];
            pic->img.i_stride[i] = ref_frame.linesize[i];
        }
    }
}

IR_XPS_EXPORT_STATUS ir_xps_import_ref_x264(x264_t* const h,
    const ir_xps_context* const xps_context, x264_picture_t* pic[2])
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

        if (NULL == pic[0])
        {
            break;
        }

        if (NULL == pic[1])
        {
            break;
        }

        h->i_threadslice_end = h->mb.i_mb_height;
        h->i_threadslice_start = 0;

        for (int k = 0; k < 2; k++)
        {
            set_ref_planes(pic[k], xps_context->ref_frame[k]);
            h->i_ref[k] = 0;
            h->fref[k][0] = NULL;
            x264_frame_t* ref = x264_frame_pop_unused(h, 1);
            if (ref != NULL)
            {
                ref->i_csp = h->fenc->i_csp;
                if (x264_frame_copy_picture(h, ref, pic[k]) == 0)
                {
                    if (h->param.i_width != 16 * h->mb.i_mb_width ||
                        h->param.i_height != 16 * h->mb.i_mb_height)
                    {
                        x264_frame_expand_border_mod16(h, ref);
                    }
                    h->fref[k][0] = ref;
                    h->fref[k][0]->i_poc = xps_context->ref_frame[k].poc;
                    h->i_ref[k]++;
                    for (int y = 0; y < h->mb.i_mb_height; y++)
                    {
                        x264_frame_expand_border(h, ref, y);
                    }
                    if (h->param.analyse.i_subpel_refine)
                    {
                        for (int y = 0; y < h->mb.i_mb_height; y++)
                        {
                            int end = ((h->mb.i_mb_height - 1) == y);
                            x264_frame_filter(h, ref, y, end);
                            x264_frame_expand_border_filtered(h, ref, y, end);
                        }
                    }
                    ref->b_kept_as_ref = 1;
                    ref->i_pic_struct = PIC_STRUCT_PROGRESSIVE;
                }
            }
            h->mb.pic.i_fref[k] = h->i_ref[k];
            h->fref_nearest[k] = h->fref[k][0];
            h->fref_nearest[k]->i_poc = h->fref[k][0]->i_poc;
        }

        result = IR_XPS_EXPORT_STATUS_OK;

    } while(0);

    return result;
}

#endif /* _IR_XPS_IMPORT_AVC_H_ */

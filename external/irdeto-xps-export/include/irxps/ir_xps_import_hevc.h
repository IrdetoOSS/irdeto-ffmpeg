/**
********************************************************************************
* @file         ir_xps_import_hevc.h
* @brief        Irdeto unified interface between decoder and encoder
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Mar 09, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/

#ifndef _IR_XPS_IMPORT_HEVC_H_
#define _IR_XPS_IMPORT_HEVC_H_

#include <stdint.h>
#include <x265.h>
#include <unistd.h>
#include <math.h>
#include <sys/sysinfo.h>

#include "ir_xps_common.h"

static void set_ref_planes(x265_param* param, x265_picture* pic,
    ir_ref ref_frame)
{
    x265_picture_init(param, pic);
    for (int i = 0; i < 3; i++)
    {
        if (ref_frame.data[i] != NULL)
        {
            pic->planes[i] = ref_frame.data[i];
            pic->stride[i] = ref_frame.linesize[i];
        }
    }
}

IR_XPS_EXPORT_STATUS ir_xps_import_ref_x265(x265_param* const param,
    x265_picture* ref0, x265_picture* ref1,
    const ir_xps_context* const xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == param)
        {
            break;
        }

        if (NULL == xps_context)
        {
            break;
        }

        if (NULL == ref0)
        {
            break;
        }

        if (NULL == ref1)
        {
            break;
        }

        if (xps_context->header.state != IR_CONTEXT_STATE_READY)
        {
            break;
        }

        set_ref_planes(param, ref0, xps_context->ref_frame[0]);
        set_ref_planes(param, ref1, xps_context->ref_frame[1]);

        result = IR_XPS_EXPORT_STATUS_OK;
    } while(0);

    return result;
}

IR_XPS_EXPORT_STATUS ir_xps_import_meta_x265(x265_param* const param,
    const ir_xps_context* const xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == param)
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

        const ir_xps_export_hevc* xps = &(xps_context->hevc_meta);

        param->bEnableWavefront = xps->pps.entropy_coding_sync_enabled;
        param->sourceWidth =  xps->sps.pic_width_in_luma_samples;
        param->sourceHeight = xps->sps.pic_height_in_luma_samples;
        param->log2MaxPocLsb = xps->sps.log2_max_pic_order_count_lsb_minus4 + 4;
        param->bEnableAMP = xps->sps.amp_enable_flag ? 1 : 0;
        param->tuQTMaxInterDepth = xps->sps.max_transform_hierarchy_depth_inter + 1;
        param->tuQTMaxIntraDepth = xps->sps.max_transform_hierarchy_depth_intra + 1;
        if (0 == xps->sps.max_transform_hierarchy_depth_inter)
        {
            param->fast_merge_level = 0;
        }
        param->levelIdc = xps->sps.general_level_idc / 3;
        param->minCUSize = (1 <<
            (xps->sps.log2_min_luma_coding_block_size_minus3 + 3));
        param->maxCUSize = (param->minCUSize <<
            xps->sps.log2_diff_max_min_luma_coding_block_size);
        param->maxTUSize = (1 <<
            (xps->sps.log2_min_luma_transform_block_size_minus2 + 2
            + xps->sps.log2_diff_max_min_luma_transform_block_size));
        int cu_rows = param->sourceHeight / param->maxCUSize;

        ///< Get number of processors configured in the operating system
        int nprocs = get_nprocs_conf();
        nprocs = (nprocs <= 0) ? 4 : nprocs;
        if(xps->pps.tiles_enabled_flag || param->bEnableWavefront)
        {
            param->maxSlices = 1;
            if (xps->pps.tiles_enabled_flag)
            {
                param->tileColumns = nprocs > (param->sourceWidth >> 8) ? (param->sourceWidth >> 8) : nprocs; // width of column must be >= 256.
            }
            param->tileRows = 1;
        }
        else
        {
            if(cu_rows < nprocs)
            {
                param->maxSlices = cu_rows;
            }
            else
            {
                param->maxSlices = nprocs;
            }

            param->maxSlices = (param->maxSlices > 8) ? 8 : param->maxSlices;
        }

        result = IR_XPS_EXPORT_STATUS_OK;
    } while(0);

    return result;
}

#endif  /* _IR_XPS_IMPORT_HEVC_H_ */

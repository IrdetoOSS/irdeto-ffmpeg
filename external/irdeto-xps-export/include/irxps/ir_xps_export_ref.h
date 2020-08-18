/**
********************************************************************************
* @file         ir_xps_export_ref.h
* @brief        Irdeto unified interface to export reference frame
* @note         This interface is only applicable for AVC for irdeto-xps-export
*               version higher or equal than 7.0, for HEVC reference frames are
*               managed at application side.
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Jul 31, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/
#ifndef INCLUDE_IRXPS_IR_XPS_EXPORT_REF_H_
#define INCLUDE_IRXPS_IR_XPS_EXPORT_REF_H_

#include <libavutil/frame.h>

#include "ir_xps_common.h"

IR_XPS_EXPORT_STATUS ir_xps_export_ref(ir_xps_context* xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == xps_context)
        {
            break;
        }

        for (int k = 0; k < IRXPS_NUM_REFS; k++)
        {
            AVFrame* frame = (AVFrame*) (xps_context->ref_frame[k].avframe);
            if (NULL != frame)
            {
                for (int i = 0; i < IRXPS_NUM_PLANES; i++)
                {
                    /* ref_frame is just a structure to remove dependency of AVFrame, encoder only aware of data and linesize */
                    xps_context->ref_frame[k].data[i] = frame->data[i];
                }
            }
        }

        result = IR_XPS_EXPORT_STATUS_OK;

    } while(0);

    return result;
}

void ir_xps_context_destroy(ir_xps_context* xps_context)
{
    if (xps_context)
    {
        int k;
        for (k = 0; k < IRXPS_NUM_REFS; k++)
        {
            AVFrame* frame = (AVFrame*) (xps_context->ref_frame[k].avframe);
            av_frame_unref(frame);
            av_frame_free(&frame);
        }

        if (xps_context->mpeg2_meta.qscale_values != NULL)
        {
        	av_free(xps_context->mpeg2_meta.qscale_values);
        }

        if (xps_context->mpeg2_meta.pkt != NULL)
        {
        	AVPacket* pkt = (AVPacket*) (xps_context->mpeg2_meta.pkt);
           	av_packet_unref(pkt);
           	av_packet_free(&pkt);
        }

        if (xps_context->avc_meta.pkt != NULL)
        {
            AVPacket* pkt = (AVPacket*) (xps_context->avc_meta.pkt);
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }

        if (xps_context->hevc_meta.pkt != NULL)
        {
            AVPacket* pkt = (AVPacket*) (xps_context->hevc_meta.pkt);
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }

        free(xps_context);
    }
}

#endif

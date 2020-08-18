/*
* Copyright (c) 2013-2017 Irdeto B.V.
*
* This file is part of FFmpeg.
*
* FFmpeg is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* FFmpeg is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _VF_WM_SPLIT_H_
#define _VF_WM_SPLIT_H_

// FFmpeg libavutil includes
#include "libavutil/opt.h"

#include <stdint.h>

// Global system includes
#include <unistd.h>
#include <stdio.h>
#include <string.h>

// FFmpeg libavutil includes
#include "libavutil/pixdesc.h"
#include "libavutil/colorspace.h"

// FFmpeg libavfilter includes
#include "config.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

/**
********************************************************************************
* @enum         VF_STATE
* @brief        Video filter states representation
********************************************************************************
*/
typedef enum
{
    VF_STATE_UNINITIALIZED  = 0,    ///< Video filter uninitialized
    VF_STATE_INITIALIZED    = 1     ///< Video filter initialized
} VF_STATE;

/**
********************************************************************************
* @struct       ir_pf_context
* @brief        Irdeto Video Filter context structure
********************************************************************************
*/
typedef struct ir_pf_context
{
    const AVClass* class;   ///< FFmpeg class pointer

    size_t x;
    size_t y;

    size_t width;
    size_t height;

    /**
    ****************************************************************************
    * @brief    Indication of the WM plugin state in current moment of time
    ****************************************************************************
    */
    VF_STATE state;

} IrdetoContext;

/**
********************************************************************************
* @def      IR_CMD_OFFSET
* @brief    List of options for the filter set through the var=value syntax by
*           the offset of the structure
********************************************************************************
*/
#define IR_CMD_OFFSET(x) offsetof(struct ir_pf_context, x)

/**
********************************************************************************
* @def      IR_CMD_FLAGS
* @brief    Command line option flags definition for Irdeto video filter
********************************************************************************
*/
#define IR_CMD_FLAGS (AV_OPT_FLAG_FILTERING_PARAM | AV_OPT_FLAG_VIDEO_PARAM)

/**
********************************************************************************
* @brief        Irdeto post filter command line options
* @note         FFmpeg video filter pipeline allows to directly pass command
*               line argument into context structure, but for other use case
*               of the plugin, please pass these parameters properly
********************************************************************************
*/
static const AVOption irdeto_wm_split_options[] =
{
    {
        "x",
        "Horizontan coordinate",
        IR_CMD_OFFSET(x),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0,
        },
        0,
        4095,
        IR_CMD_FLAGS
    },
    {
        "y",
        "Vertical coordinate",
        IR_CMD_OFFSET(y),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0
        },
        0,
        3071,
        IR_CMD_FLAGS
    },
    {
        "w",
        "Width in pixels",
        IR_CMD_OFFSET(width),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0,
        },
        0,
        4096,
        IR_CMD_FLAGS
    },
    {
        "h",
        "Height in pixels",
        IR_CMD_OFFSET(height),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0
        },
        0,
        3072,
        IR_CMD_FLAGS
    },
    { NULL }
};

AVFILTER_DEFINE_CLASS(irdeto_wm_split);

/**
* @brief        Collect basic information about environemnt and store in AVFilterContext
* @param        [in,out] ctx    AVFilter context will be initialized
* @return       0 on success, ENOENT or ENOSYS otherwise
*/
static av_cold int wm_plugin_init(AVFilterContext* const ctx)
{
    int result = 0;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    int i = 0;

    do
    {
        context->state       = VF_STATE_UNINITIALIZED;
    } while(0);

    return AVERROR(result);
}

/**
* @brief        Perform post-initialization of the AVFilter where WM detection library is actually initialized. This
*               function should be called when at least first frame is available
* @param        [in] inlink Input context for AVFilter
* @param        [in] frame  Frame pointer which should be processed
* @return       0 on success, error code - otherwise
*/
static int wm_plugin_postinit(AVFilterLink* const inlink, const AVFrame* const frame)
{
    int result = 0;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    
    do
    {
        // In case filter is initialized imediately exit and continue processing
        if (VF_STATE_INITIALIZED == context->state)
        {
            break;
        }



        context->state = VF_STATE_INITIALIZED;

    } while(0);
    
    return result;
}

/**
* @brief        Process single frame coming out of decoder or previous filter if applicable
* @param        [in] inlink     AVFilter input connection
* @param        [in] frame      Frame to be processed
* @return       0 on success, error code - otherwise
*/
static int wm_plugin_process_frame(AVFilterLink* const inlink, AVFrame* const frame)
{
    int result = -1;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    size_t i, j;

    do
    {   
        // Exit immediately if initialized
        if (VF_STATE_UNINITIALIZED == context->state)
        {
            break;
        }

        if (context->x >= frame->width - 1 || context->y >= frame->height - 1)
        {
        	av_log(ctx, AV_LOG_WARNING, "Coordinates (%zu:%zu) bigger than resolution (%zu:%zu)\n",
        		context->x, context->y, frame->width, frame->height);
        	break;
        }

        if (context->x + context->width > frame->width)
        {
        	av_log(ctx, AV_LOG_WARNING, "Box goes out of the frame in horizontal\n");
        	break;
        }

        if (context->y + context->height > frame->height)
        {
        	av_log(ctx, AV_LOG_WARNING, "Box goes out of the frame in horizontal\n");
        	break;
        }

        av_frame_make_writable(frame);

        uint8_t* ptr = frame->data[0];
        ptr[0] = 0;
        // Check wheather we have 10 bit or 8 bit image
        if (frame->linesize[0] >= frame->width * 2)
        {
        	for (i = context->y; i < context->y + context->height; i++)
        	{
        		memset(ptr + (i * frame->linesize[0] + context->x), 0, context->width * 2);
        	}
        }
        else
        {
        	for (i = context->y; i < context->y + context->height; i++)
        	{
        		memset(ptr + (i * frame->linesize[0] + context->x), 0, context->width);
        	}
        }

        result = 0;
    } while(0);

    return result;
}

/**
* @brief        Filter single frame througn processing
* @param        [in] inlink     AVFilter input connection
* @param        [in] frame      Frame to be processed
* @return       0 on success, error code otherwise
*/
static int wm_plugin_filter_frame(AVFilterLink* const inlink,
    AVFrame* const frame)
{
    int result;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;

    do
    {
        
        result = wm_plugin_postinit(inlink, frame);
        if (0 != result)
        {
            result = EPERM;
            break;
        }

        result = wm_plugin_process_frame(inlink, frame);
        if (0 != result)
        {
            result = EPERM;
            break;
        }

        result = ff_filter_frame(ctx->outputs[0], frame);
    } while(0);

    return AVERROR(result);
}

/**
* @brief        Uninitialize filter and free all resources
* @param        [in] ctx    AVFilter context
* @return       void
*/
static av_cold void wm_plugin_uninit(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        // Exit immediately if initialized
        if (VF_STATE_UNINITIALIZED == context->state)
        {
            break;
        }

    } while(0);

    av_log(ctx, AV_LOG_INFO, "Filter %s was uninitialized\n", ctx->filter->name);
}

static const AVFilterPad wm_plugin_inputs[] =
{
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .get_video_buffer = ff_null_get_video_buffer,
        .filter_frame     = wm_plugin_filter_frame,
        .config_props     = NULL,
        .needs_writable   = 0,
    },
    { NULL }
};

static const AVFilterPad wm_plugin_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

static int wm_plugin_register_formats(AVFilterContext* const ctx)
{
    enum AVPixelFormat pix_fmts[] =
    {
        AV_PIX_FMT_YUV420P10,
        AV_PIX_FMT_YUV422P10,
        AV_PIX_FMT_YUV444P10,
        AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV422P,
        AV_PIX_FMT_YUV444P,
        AV_PIX_FMT_NONE
    };

    return ff_set_common_formats(ctx, ff_make_format_list(pix_fmts));
}

AVFilter ff_vf_irdeto_wm_split =
{
    .name            = "irdeto_wm_split",
    .description     = NULL_IF_CONFIG_SMALL("Irdeto Video WM split filter"),
    .priv_size       = sizeof(struct ir_pf_context),
    .priv_class      = &irdeto_wm_split_class,
    .init            = wm_plugin_init,
    .uninit          = wm_plugin_uninit,
    .query_formats   = wm_plugin_register_formats,
    .inputs          = wm_plugin_inputs,
    .outputs         = wm_plugin_outputs,
    .process_command = NULL,
};

#endif	/* !_VF_WM_SPLIT_H_ */

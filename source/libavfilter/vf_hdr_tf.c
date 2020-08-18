/*
* Copyright (c) 2013-2016 Irdeto B.V.
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

// Global system includes
#include <unistd.h>
#include <dlfcn.h>
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

// Irdeto HDR transfer function AVFilter command line include
#include "vf_hdr_tf_ctx.h"

/**
********************************************************************************
* @brief        Set Irdeto filter class name
********************************************************************************
*/
AVFILTER_DEFINE_CLASS(irhdrtf);

static void ir_print_config(AVFilterContext* const ctx)
{
    struct ir_hdr_context* context = ctx->priv;
    av_log(ctx, AV_LOG_INFO,
            "Irdeto HDR transfer function filter configuration\n"
            "\tTransfer function type: %s\n"
            "\tNormalization base value: %d\n"
            "\tFrame property: %zux%zu:%zu\n",
            (context->config.tf_type == 0) ? "EOTF" : "OETF",
            context->config.norm_base,
            context->width, context->height, context->linesize);
}

static av_cold int vf_irhdrtf_init(AVFilterContext* const ctx)
{
    struct ir_hdr_context* context = (struct ir_hdr_context*) ctx->priv;
    int sys = 0;

    do
    {
        if (0 >= context->config.norm_base)
        {
            sys = EINVAL;
            break;
        }

        if (0 != context->config.tf_type && 1 != context->config.tf_type)
        {
            sys = EINVAL;
            break;
        }

        context->state = VF_STATE_UNINITIALIZED;

    } while(0);

    return AVERROR(sys);
}

static int vf_irhdrtf_postinit(AVFilterLink* const inlink,
    AVFrame* const frame)
{
    int result = 1;
    AVFilterContext* filter         = (AVFilterContext*) inlink->dst;
    struct ir_hdr_context* context   = (struct ir_hdr_context*) filter->priv;

    do
    {
        if (VF_STATE_INITIALIZED == context->state) // Early exit when done
        {
            result = 0;
            break;
        }

        context->width       = frame->width;
        context->height      = frame->height;
        context->linesize    = frame->linesize[0];
        context->output_buffer = (uint8_t*) malloc(context->linesize *
            context->height);
        if (NULL == context->output_buffer)
        {
            break;
        }

        ir_print_config(filter);

        context->state = VF_STATE_INITIALIZED;
        result = 0;

    } while(0);

    return result;
}

static const double alpha = 1.09929682680944;
static const double alpha_minus_1 = 0.09929682680944;
static const double beta = 0.018053968510807;

static double oetf_unit(double E)
{
    double E_prime = 0.0;
    if (E < beta)
    {
        E_prime = 4.5 * E;
    }
    else
    {
        E_prime = alpha * pow(E, 0.45) - alpha_minus_1;
    }

    return E_prime;
}

static double eotf_unit(double E_prime)
{
    double E = 0.0;
    if (E_prime <= 4.5 * beta)
    {
        E = E_prime / 4.5;
    }
    else
    {
        E = pow(((alpha_minus_1 + E_prime) / alpha), 1.0 / 0.45);
    }

    return E;
}
static int ir_hdr_eotf(const uint16_t* const input, uint16_t* const output,
    size_t size)
{
    size_t i = 0;

    for (i = 0; i < size; i++)
    {
        output[i] = (uint16_t) (eotf_unit((double)(input[i]) / 1023.0) * 1023);
    }

    return 0;
}

static int ir_hdr_oetf(const uint16_t* const input, uint16_t* const output,
    size_t size)
{
    size_t i = 0;

    for (i = 0; i < size; i++)
    {
        output[i] = (uint16_t) (oetf_unit((double)(input[i]) / 1023.0) * 1023);
    }

    return 0;
}

typedef int (*ir_hdr_fptr) (const uint16_t* const input, uint16_t* const
    output, size_t size);

static ir_hdr_fptr s_hdrtf[2] =
{
    ir_hdr_eotf,
    ir_hdr_oetf
};

static int ir_hdr_transfer(const void* const input, void* const output,
    size_t width, size_t height, size_t linesize, int norm_base, int tf_type)
{
    int result = 1;

    do
    {
        if (NULL == input)
        {
            break;
        }

        if (NULL == output)
        {
            break;
        }

        if (0 == width)
        {
            break;
        }

        if (0 == height)
        {
            break;
        }

        if (0 == linesize)
        {
            break;
        }

        uint16_t* i_pic = (uint16_t*) input;
        uint16_t* o_pic = (uint16_t*) output;

        result = s_hdrtf[tf_type](i_pic, o_pic, width * height);

    } while(0);

    return result;
}

static int vf_irhdrtf_process_frame(AVFilterLink* const inlink, 
    AVFrame* const frame)
{
    int result = 1;
    AVFilterContext* filter = (AVFilterContext*) inlink->dst;
    struct ir_hdr_context* context = (struct ir_hdr_context*) filter->priv;
    
    do
    {
        av_frame_make_writable(frame);

        result = ir_hdr_transfer(frame->data[0], context->output_buffer,
            context->width, context->height, context->linesize,
            context->config.norm_base, context->config.tf_type);

        if (0 != result)
        {
            break;
        }

        memcpy(frame->data[0], context->output_buffer, context->linesize *
            context->height);
        
    } while(0);

    return result;
}

static int ff_irhdrtf_filter_frame(AVFilterLink* const inlink,
    AVFrame* const frame)
{
    int sys = 0;
    int result = 0;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
    
    do
    {
        result = vf_irhdrtf_postinit(inlink, frame);
        if (0 != result)
        {
            sys = EPERM;
            break;
        }

        result = vf_irhdrtf_process_frame(inlink, frame);
        if (0 != result)
        {
            sys = EPERM;
            break;
        }

        sys = ff_filter_frame(ctx->outputs[0], frame);
        
    } while(0);

    return AVERROR(sys);
}

static av_cold void vf_irhdrtf_uninit(AVFilterContext* const ctx)
{
    struct ir_hdr_context* context = ctx->priv;

    if (NULL != context && VF_STATE_INITIALIZED == context->state)
    {
        free(context->output_buffer);
    }

    context->state = VF_STATE_UNINITIALIZED;
    av_log(ctx, AV_LOG_INFO, "Irdeto FFmpeg Video filter was uninitialized");
}

/**
********************************************************************************
* @brief    Connector for the filter inputs
********************************************************************************
*/
static const AVFilterPad ff_vf_irhdrtf_inputs[] =
{
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .get_video_buffer = ff_null_get_video_buffer,
        .filter_frame     = ff_irhdrtf_filter_frame,
        .config_props     = NULL,
        .needs_writable   = 1,
    },
    { NULL }
};

/**
********************************************************************************
* @brief    Connector for the filter outputs
********************************************************************************
*/
static const AVFilterPad ff_vf_irhdrtf_outputs[] =
{
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

static int ir_irhdrtf_register_formats(AVFilterContext* const ctx)
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

/**
********************************************************************************
* @brief    Irdeto HDR transfer function Filter definition
********************************************************************************
*/
AVFilter ff_vf_irhdrtf =
{
    .name            = "irhdrtf",
    .description     = NULL_IF_CONFIG_SMALL("Irdeto custom HDR FFmpeg filter"),
    .priv_size       = sizeof(struct ir_hdr_context),
    .priv_class      = &irhdrtf_class,
    .init            = vf_irhdrtf_init,
    .uninit          = vf_irhdrtf_uninit,
    .query_formats   = ir_irhdrtf_register_formats,
    .inputs          = ff_vf_irhdrtf_inputs,
    .outputs         = ff_vf_irhdrtf_outputs,
};

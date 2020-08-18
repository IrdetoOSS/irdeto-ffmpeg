/*
* Copyright (c) 2018 Irdeto B.V.
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

// Irdeto detection AVFilter context and basic include files
#include "vf_cb_det_ctx.h"


/**
* @def          IR_CBWM_LIB
* @brief        Absolute path to Detection library used by default (production environment)
*/
#define IR_CBWM_LIB "/usr/lib64/ircbwm/libircbwmd.so"

#define FILTER_NAME "irdeto_cbwm_det"

AVFILTER_DEFINE_CLASS(irdeto_cbwm_det);

/**
* @brief        Convert FFmpeg AVPixelFormat to Irdeto Pixel Format enumeration
* @param        [in] format     FFmpeg pixel format value
* @see          wm_plugin_register_formats initial filtering supported by FFmpeg
* @return       IR_WMD_PF value
*/
static IR_WMD_PF ir_wmd_convert_pf(const enum AVPixelFormat format)
{
    IR_WMD_PF pixfmt;

    switch(format)
    {
        case AV_PIX_FMT_YUV420P:
            pixfmt = IR_WMD_PF_YUV420;
            break;

        case AV_PIX_FMT_YUV422P:
            pixfmt = IR_WMD_PF_YUV422;
            break;

        case AV_PIX_FMT_YUV444P:
            pixfmt = IR_WMD_PF_YUV444;
            break;

        case AV_PIX_FMT_YUV420P10:
            pixfmt = IR_WMD_PF_YUV420_10BIT;
            break;

        case AV_PIX_FMT_YUV422P10:
            pixfmt = IR_WMD_PF_YUV422_10BIT;
            break;

        case AV_PIX_FMT_YUV444P10:
            pixfmt = IR_WMD_PF_YUV444_10BIT;
            break;
    }

    return pixfmt;
}

/**
* @brief        Load detection library in runtime on every start of a filter
* @param        [in,out] ctx    AVfilter context will be used to load library
* @return       0 on success, 12 - otherwise
*/
static int ir_load_wm_plugin(AVFilterContext* const ctx)
{
    int result = 12;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        // Check environmental variable first and if it exists to use it instead of default
        char* plugin_path = getenv("IR_CBWM_DET_LIB");
        if (NULL == plugin_path)
        {
            plugin_path = (char*) IR_CBWM_LIB;
        }

        av_log(ctx, AV_LOG_INFO, "Watermarking plugin %s will be used\n", plugin_path);
        context->handle = dlopen(plugin_path, RTLD_NOW | RTLD_NODELETE);
        if (NULL == context->handle)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load WM plugin %s. Error: %s\n", plugin_path, dlerror());
            break;
        }

        context->wmd_init = (f_cbwmd_init) dlsym(context->handle, "ir_cbwmd_init");
        if (NULL == context->wmd_init)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_init function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_extract = (f_cbwmd_extract) dlsym(context->handle, "ir_cbwmd_extract");
        if (NULL == context->wmd_extract)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_extract function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_uninit = (f_cbwmd_uninit) dlsym(context->handle, "ir_cbwmd_uninit");
        if (NULL == context->wmd_uninit)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_uninit function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_version = (f_cbwmd_get_version) dlsym(context->handle, "ir_cbwmd_get_version");
        if (NULL == context->wmd_version)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_get_version function. Error: %s\n", dlerror());
            break;
        }

        result = 0;
    } while(0);

    return result;
}

/**
* @brief        Print to FFmpeg log parameters will be used for Watermark detection
* @param        [in] ctx    AVfilter context will be used to load library
* @return       void
*/
static void ir_dump_config(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    av_log(ctx, AV_LOG_INFO,
        "Irdeto %s configuration:\n"
        "Version: %s\n"
        "Nodes:%" PRIu64 "\n"
        "Mode: %s\n"
        "Endianness: %s\n"
        "Inversion: %u\n"
        "Summary: %s\n",
        ctx->filter->name, context->wmd_version(), context->nodes,
        context->mode == 0 ? "regular" : "exhaustive",
        ((NULL != context->endian && 0 == strcmp("be", context->endian)) ? "big endian" : "little endian"),
        (unsigned int) context->invert,
        context->summary);
}


/**
* @brief        Collect basic information about environemnt and store in AVFilterContext
* @param        [in,out] ctx    AVFilter context will be initialized
* @return       0 on success, ENOENT or ENOSYS otherwise
*/
static av_cold int wm_plugin_init(AVFilterContext* const ctx)
{
    int result = 0;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        // Load plugin from file to memory
        if (0 != ir_load_wm_plugin(ctx))
        {
            result = ENOENT;
            break;
        }
        context->type = 0;
        context->frame_count = 0;
        context->state       = VF_STATE_UNINITIALIZED;
        context->prog_flag   = strncmp("off", context->progress, 3);

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
        CBWM_STATUS det_result;
        struct ir_wmd_config config;

        // In case filter is initialized imediately exit and continue processing
        if (VF_STATE_INITIALIZED == context->state)
        {
            break;
        }

        config.mode             = context->mode;
        config.cores            = (size_t) context->nodes;
        config.fpi              = (size_t) context->fpi;
        config.width            = frame->width;
        config.height           = frame->height;
        config.linesize         = frame->linesize[0];
        config.fps              = inlink->frame_rate.num / (float) inlink->frame_rate.den;
        config.pf               = ir_wmd_convert_pf(frame->format);
        config.flags.invert     = context->invert;
        config.flags.big_endian = ((NULL != context->endian && 0 == strcmp("be", context->endian)) ? 1 : 0);

        // Print configuration only once and set flag that filter and library now initialized
        ir_dump_config(ctx);

        det_result = context->wmd_init(&config, &context->wmd_ctx);

        if (CBWM_STATUS_OK != det_result)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't init Irdeto WM detection plugin. (Error: %d)\n", det_result);
            result = -1;
            break;
        }

        context->s_pts          = frame->pts * av_q2d(inlink->time_base);
        context->frame_count    = context->s_pts / config.fps;

        context->state = VF_STATE_INITIALIZED; //prevent initializing more than once

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
    uint32_t cssn = 0;
    int detected = 0;

    do
    {
        CBWM_STATUS det_result;

        // Exit immediately if initialized
        if (VF_STATE_UNINITIALIZED == context->state)
        {
            break;
        }

        // Extract cssn from frame
        det_result = context->wmd_extract(context->wmd_ctx, frame->data[0], &detected, &cssn);

        if ((CBWM_STATUS_AVG_IN_PROCESS != det_result) && (CBWM_STATUS_OK != det_result))
        {
            av_log(ctx, AV_LOG_ERROR, "Can't process frame %llu. (Error: %d)\n", (unsigned long long) context->frame_count, (int) det_result);
            result = -1;
            break;
        }

        // Print detection result
        if ((CBWM_STATUS_OK == det_result) && (detected == 1))
        {
            /**
             * @note: For encoding details, please see the link below:
             *          https://irdeto.atlassian.net/wiki/spaces/CSMUW/pages/1440024040/OWL+Identities+-+Split+Between+Head-end+and+Client
             */
            uint64_t payload         = 0x0000000000000000;
            uint32_t overlay_type_id = 0x09; /** b1001 -> OWLO DVB STB-client */

            payload |= (cssn & 0xFC000000) >> 26;
            payload |= overlay_type_id << 6;
            payload |= ((uint64_t) (cssn & 0x03FFFFFF)) << 10;

            av_log(ctx, AV_LOG_INFO, "[Detected: FN: %.16llu, WM: %09" PRIx64 ", Overlay Type: %u, Device ID: %u]\n",
                (long long unsigned) context->frame_count, payload, overlay_type_id, cssn);

            //av_log(ctx, AV_LOG_INFO, "CSSN Number: 0x%08x]\n", cssn);
        }

        result = 0;
    } while(0);

    context->frame_count++;
    if (context->prog_flag && 0 == (context->frame_count % 500)) av_log(ctx, AV_LOG_INFO, "Frame number processed: %llu !\n", (unsigned long long) context->frame_count);
    context->f_pts = frame->pts * av_q2d(inlink->time_base); // Seconds
    return result;
}

/**
* @brief        Filter single frame through detection pipeline
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
    int s_min, s_hours, s_sec;
    int f_min, f_hours, f_sec;

    do
    {
        // Exit immediately if initialized
        if (VF_STATE_UNINITIALIZED == context->state)
        {
            break;
        }

    } while(0);

    context->frame_count++;

    //uninit detector and close library
    context->wmd_uninit(context->wmd_ctx);
    if (context->handle)
    {
        dlclose(context->handle);
    }

    s_min   = context->s_pts / 60;
    s_hours = s_min / 60;
    s_sec   = context->s_pts - (((s_hours * 60) + s_min) * 60);
    s_min = s_min - (s_hours * 60);

    f_min   = context->f_pts / 60;
    f_hours = f_min / 60;
    f_sec   = context->f_pts - (((f_hours * 60) + f_min) * 60);
    f_min = f_min - (f_hours * 60);

    av_log(ctx, AV_LOG_INFO, "Processing summary: S(%.2d:%.2d:%.2d), F(%.2d:%.2d:%.2d)\n",
        s_hours, s_min, s_sec, f_hours, f_min, f_sec);

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

AVFilter ff_vf_irdeto_cbwm_det =
{
    .name            = FILTER_NAME,
    .description     = NULL_IF_CONFIG_SMALL("Irdeto Client-Based Watermark detection plugin"),
    .priv_size       = sizeof(struct ir_pf_context),
    .priv_class      = &irdeto_cbwm_det_class,
    .init            = wm_plugin_init,
    .uninit          = wm_plugin_uninit,
    .query_formats   = wm_plugin_register_formats,
    .inputs          = wm_plugin_inputs,
    .outputs         = wm_plugin_outputs,
    .process_command = NULL,
};

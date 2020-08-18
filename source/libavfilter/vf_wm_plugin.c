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

// Irdeto AVFilter command line include
#include "vf_wm_plugin_cmd.h"

#define IR_WM_LIB "/usr/lib64/irwmp/libirwmp.so"
#define IR_WM_PLUGIN_PATH "/usr/lib64/irwmp"
#define IR_LICENSE "/etc/irffmpeg/plugin.license"

AVFILTER_DEFINE_CLASS(irdeto_owl_emb);

static const char* ir_wmp_convert_pf(enum AVPixelFormat format)
{
    const char* pixfmt;

    switch(format)
    {
        case AV_PIX_FMT_YUV420P:
        {
            pixfmt = "yuv420p";
            break;
        }
        case AV_PIX_FMT_YUV422P:
        {
            pixfmt = "yuv422p";
            break;
        }
        case AV_PIX_FMT_YUV444P:
        {
            pixfmt = "yuv444p";
            break;
        }
        case AV_PIX_FMT_YUV420P10:
        {
            pixfmt = "yuv420p10le";
            break;
        }
        case AV_PIX_FMT_YUV422P10:
        {
            pixfmt = "yuv422p10le";
            break;
        }
        case AV_PIX_FMT_YUV444P10:
        {
            pixfmt = "yuv444p10le";
            break;
        }
        default:
        {
            pixfmt = "null";    ///< null means unset
            break;
        }
    }

    return pixfmt;
}

static int ir_load_wm_plugin(AVFilterContext* const ctx)
{
    int result = 12;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        char* plugin_path = getenv("IR_WM_LIB");
        if (NULL == plugin_path)
        {
            plugin_path = (char*) IR_WM_LIB;
        }

        av_log(ctx, AV_LOG_DEBUG, "Watermarking plugin %s will be used\n",
            plugin_path);
        context->wmp_handle = dlopen(plugin_path, RTLD_NOW | RTLD_NODELETE);
        if (NULL == context->wmp_handle)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load WM plugin %s. Error: %s\n",
                plugin_path, dlerror());
            break;
        }

        context->wmp_init = (f_wmp_init) dlsym(context->wmp_handle, "ir_wmp_init");
        if (NULL == context->wmp_init)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_init function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_setattr = (f_wmp_setattr) dlsym(context->wmp_handle,
            "ir_wmp_setattr");
        if (NULL == context->wmp_setattr)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_setattr function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_configure = (f_wmp_configure) dlsym(context->wmp_handle,
            "ir_wmp_configure");
        if (NULL == context->wmp_configure)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_configure function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_embed = (f_wmp_embed) dlsym(context->wmp_handle,
            "ir_wmp_embed");
        if (NULL == context->wmp_embed)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_embed function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_uninit = (f_wmp_uninit) dlsym(context->wmp_handle,
            "ir_wmp_uninit");
        if (NULL == context->wmp_uninit)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_uninit function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_getattr = (f_wmp_getattr) dlsym(context->wmp_handle,
            "ir_wmp_getattr");
        if (NULL == context->wmp_getattr)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_getattr function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_version = (f_wmp_version) dlsym(context->wmp_handle,
            "ir_wmp_get_version");
        if (NULL == context->wmp_version)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_get_version function. Error: %s\n",
                dlerror());
            break;
        }

        context->wmp_itos = (f_wmp_itos) dlsym(context->wmp_handle,
            "ir_wmp_int_to_str");
        if (NULL == context->wmp_itos)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_int_to_str function. Error: %s\n",
                dlerror());
            break;
        }

        context->wmp_ftos = (f_wmp_ftos) dlsym(context->wmp_handle,
            "ir_wmp_float_to_str");
        if (NULL == context->wmp_ftos)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't load ir_wmp_float_to_str function. Error: %s\n",
                dlerror());
            break;
        }

        result = 0;
    } while(0);

    return result;
}

static void ir_dump_config(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    av_log(ctx, AV_LOG_INFO,
        "Irdeto WM filter configuration:\n"
        "Video property: %s %sx%s:%s@%sfps\n"
        "Profile: %s\n"
        "Operator: %s\n"
        "TMID: %s\n"
        "Debug banner: %s\n"
        "Logfile: %s, loglevel: %s\n"
        "Version: %s\n",
        context->wmp_getattr(context->wmp_ctx, "pixfmt"),
        context->wmp_getattr(context->wmp_ctx, "width"),
        context->wmp_getattr(context->wmp_ctx, "height"),
        context->wmp_getattr(context->wmp_ctx, "linesize"),
        context->wmp_getattr(context->wmp_ctx, "fps"),
        context->wmp_getattr(context->wmp_ctx, "profile"),
        context->wmp_getattr(context->wmp_ctx, "operator"),
        context->wmp_getattr(context->wmp_ctx, "tmid"),
        context->wmp_getattr(context->wmp_ctx, "banner"),
        context->wmp_getattr(context->wmp_ctx, "logfile"),
        context->wmp_getattr(context->wmp_ctx, "loglevel"),
        context->wmp_version());
}

static av_cold int wm_plugin_init(AVFilterContext* const ctx)
{
    int result = 0;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        if (0 != ir_load_wm_plugin(ctx))
        {
            result = ENOENT;
            break;
        }

        context->state = VF_STATE_UNINITIALIZED;

    } while(0);

    return AVERROR(result);
}

static int wm_plugin_postinit(AVFilterLink* const inlink, AVFrame* const frame)
{
    int result = 0;
    char value[64];
    float fps;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        if (VF_STATE_INITIALIZED == context->state)
        {
            break;
        }

        char* plugin_path = getenv("IR_WM_PLUGIN_PATH");
        if (NULL == plugin_path)
        {
            plugin_path = (char*) IR_WM_PLUGIN_PATH;
        }

        result = context->wmp_init("6c7e4932-8256-11e7-a393-5065f34e54a8", plugin_path, &context->wmp_ctx);
        if (0 != result)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't init Irdeto WM plugin. Error: %d\n", result);
            break;
        }

        char* license_file = getenv("IR_LICENSE");
        if (NULL == license_file)
        {
            license_file = (char*) IR_LICENSE;
        }

        context->wmp_setattr(context->wmp_ctx, "license", license_file);

        context->wmp_itos(frame->width, value, 64);
        context->wmp_setattr(context->wmp_ctx, "width", value);

        context->wmp_itos(frame->height, value, 64);
        context->wmp_setattr(context->wmp_ctx, "height", value);

        context->wmp_itos(frame->linesize[0], value, 64);
        context->wmp_setattr(context->wmp_ctx, "linesize", value);

        context->wmp_setattr(context->wmp_ctx, "pixfmt",
            ir_wmp_convert_pf(inlink->format));

        fps = inlink->frame_rate.num / (float) inlink->frame_rate.den;
        context->wmp_ftos(fps, value, 64);
        context->wmp_setattr(context->wmp_ctx, "fps", value);

        ///< Watermark related attributes
        context->wmp_setattr(context->wmp_ctx, "operator", context->oid);
        context->wmp_setattr(context->wmp_ctx, "tmid", context->tmid);
        context->wmp_setattr(context->wmp_ctx, "firstbit", context->firstbit);
        context->wmp_setattr(context->wmp_ctx, "lastbit", context->lastbit);
        context->wmp_setattr(context->wmp_ctx, "firstframe",
            context->firstframe);
        context->wmp_setattr(context->wmp_ctx, "lastframe", context->lastframe);
        context->wmp_setattr(context->wmp_ctx, "profile", context->profile);

        ///< Debug attributes, note that loglevel is set to debug on purpose
        context->wmp_setattr(context->wmp_ctx, "banner", context->banner);
        context->wmp_setattr(context->wmp_ctx, "logfile", context->logfile);
        context->wmp_setattr(context->wmp_ctx, "loglevel", "debug");

        ///< Customized plugin attributes
        context->wmp_setattr(context->wmp_ctx, "wmtime", context->wmtime);

        ir_dump_config(ctx);

        result = context->wmp_configure(context->wmp_ctx);
        if (0 != result)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't configure Irdeto WM plugin. Error: %d\n", result);
            break;
        }

        context->state = VF_STATE_INITIALIZED;

    } while(0);

    return result;
}

static int wm_plugin_process_frame(AVFilterLink* const inlink,
    AVFrame* const frame)
{
    int result = -1;
    char value[64];
    unsigned long long pts;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        av_frame_make_writable(frame);
        pts = frame->pts * av_q2d(inlink->time_base) * 1000;
        context->wmp_itos(pts, value, 64);
        result = context->wmp_embed(context->wmp_ctx, frame->data[0],
            "pts", value);
        if (result < 0)
        {
            av_log(ctx, AV_LOG_ERROR,
                "Can't embed watermark to frame %d. Error: %d\n",
                frame->display_picture_number, result);
            break;
        }

        result = 0;

    } while(0);

    return result;
}

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
            result = 0;
            break;
        }

        result = ff_filter_frame(ctx->outputs[0], frame);
    } while(0);

    return AVERROR(result);
}

static av_cold void wm_plugin_uninit(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    context->wmp_uninit(context->wmp_ctx);
    if (context->wmp_handle)
    {
        dlclose(context->wmp_handle);
    }

    av_log(ctx, AV_LOG_INFO, "Irdeto WM filter uninitialized\n");
}

static int wm_plugin_pc(AVFilterContext* const ctx, const char* const cmd,
    const char* const arg, char* const res, int size, int flags)
{
    int result = 0;

    do
    {
        if (strcmp(cmd, "reinit") != 0)
        {
            result = AVERROR(ENOSYS);
            break;
        }

        wm_plugin_uninit(ctx);
        result = wm_plugin_init(ctx);

    } while(0);

    return result;
}

static const AVFilterPad wm_plugin_inputs[] =
{
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .get_video_buffer = ff_null_get_video_buffer,
        .filter_frame     = wm_plugin_filter_frame,
        .config_props     = NULL,
        .needs_writable   = 1,
    },
    { NULL }
};

static const AVFilterPad wm_plugin_outputs[] =
{
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

/**
********************************************************************************
* @brief    Frame Based embedding Filter registration
********************************************************************************
*/
AVFilter ff_vf_irdeto_owl_emb =
{
    .name            = "irdeto_owl_emb",
    .description     = NULL_IF_CONFIG_SMALL("Irdeto OWL watermark embedding plugin"),
    .priv_size       = sizeof(struct ir_pf_context),
    .priv_class      = &irdeto_owl_emb_class,
    .init            = wm_plugin_init,
    .uninit          = wm_plugin_uninit,
    .query_formats   = wm_plugin_register_formats,
    .inputs          = wm_plugin_inputs,
    .outputs         = wm_plugin_outputs,
    .process_command = wm_plugin_pc,
};

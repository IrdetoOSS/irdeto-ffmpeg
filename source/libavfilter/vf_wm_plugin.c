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
#include <time.h>

// FFmpeg libavutil includes
#include "libavutil/pixdesc.h"
#include "libavutil/colorspace.h"
#include "libavutil/timecode.h"
#include "libavutil/ir_wm_info.h"

// FFmpeg libavfilter includes
#include "config.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

// Irdeto AVFilter command line include
#include "vf_wm_plugin_cmd.h"

#ifdef IR_LIB_PATH
    #define STR_INDIR(x)      #x
    #define STR(x)            STR_INDIR(x)

    #define IR_WM_PLUGIN_PATH STR(IR_LIB_PATH) "/irwmp"
    #define IR_WM_LIB         STR(IR_LIB_PATH) "/irwmp/libirwmp.so"
#else
    #define IR_WM_PLUGIN_PATH "/usr/lib64/irwmp"
    #define IR_WM_LIB         "/usr/lib64/irwmp/libirwmp.so"
#endif

#ifndef IR_LICENSE
    #define IR_LICENSE "/etc/irffmpeg/plugin.license"
#endif

#define IR_WM_DRID_UUID   "92ea7f64-07ba-11ea-94a2-8b6bf42447b2"
#define IR_WM_TMID_UUID   "6c7e4932-8256-11e7-a393-5065f34e54a8"
#define IR_WM_SEQID_UUID  "e5492eda-8716-11e7-a214-5065f34e54a8"

AVFILTER_DEFINE_CLASS(irdeto_owl_emb);

static const char* ir_wmp_convert_pf(enum AVPixelFormat format)
{
    const char* pixfmt;

    switch(format)
    {
        // 8-bit pixel formats
        case AV_PIX_FMT_GRAY8:
        {
            pixfmt = "yuv400p"; // "gray", "gray8", "y8"
            break;
        }
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

        // 10-bit pixel formats
        case AV_PIX_FMT_GRAY10LE:    // AV_PIX_FMT_GRAY10
        {
            pixfmt = "yuv400p10le";  // "gray10le", "y10le"
            break;
        }
        case AV_PIX_FMT_YUV420P10LE: // AV_PIX_FMT_YUV420P10
        {
            pixfmt = "yuv420p10le";
            break;
        }
        case AV_PIX_FMT_YUV422P10LE: // AV_PIX_FMT_YUV422P10
        {
            pixfmt = "yuv422p10le";
            break;
        }
        case AV_PIX_FMT_YUV444P10LE: // AV_PIX_FMT_YUV444P10
        {
            pixfmt = "yuv444p10le";
            break;
        }

        // 12-bit pixel formats
        case AV_PIX_FMT_GRAY12LE:    // AV_PIX_FMT_GRAY12
        {
            pixfmt = "yuv400p12le";  // "gray12le", "y12le"
            break;
        }
        case AV_PIX_FMT_YUV420P12LE: // AV_PIX_FMT_YUV420P12
        {
            pixfmt = "yuv420p12le";
            break;
        }
        case AV_PIX_FMT_YUV422P12LE: // AV_PIX_FMT_YUV422P12
        {
            pixfmt = "yuv422p12le";
            break;
        }
        case AV_PIX_FMT_YUV444P12LE: // AV_PIX_FMT_YUV444P12
        {
            pixfmt = "yuv444p12le";
            break;
        }

        // Unsupported formats
        default:
        {
            pixfmt = "null"; // null means unset
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

        av_log(ctx, AV_LOG_DEBUG, "Watermarking plugin %s will be used\n", plugin_path);
        context->wmp_handle = dlopen(plugin_path, RTLD_NOW | RTLD_NODELETE);
        if (NULL == context->wmp_handle)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load WM plugin %s. Error: %s\n", plugin_path, dlerror());
            break;
        }

        context->wmp_init = (f_wmp_init) dlsym(context->wmp_handle, "ir_wmp_init");
        if (NULL == context->wmp_init)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_init function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_setattr = (f_wmp_setattr) dlsym(context->wmp_handle, "ir_wmp_setattr");
        if (NULL == context->wmp_setattr)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_setattr function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_configure = (f_wmp_configure) dlsym(context->wmp_handle, "ir_wmp_configure");
        if (NULL == context->wmp_configure)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_configure function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_embed = (f_wmp_embed) dlsym(context->wmp_handle, "ir_wmp_embed");
        if (NULL == context->wmp_embed)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_embed function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_sei_payload = (f_wmp_sei_payload) dlsym(context->wmp_handle, "ir_wmp_generate_sei_payload");
        if (NULL == context->wmp_sei_payload)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_generate_sei_payload function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_uninit = (f_wmp_uninit) dlsym(context->wmp_handle, "ir_wmp_uninit");
        if (NULL == context->wmp_uninit)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_uninit function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_getattr = (f_wmp_getattr) dlsym(context->wmp_handle, "ir_wmp_getattr");
        if (NULL == context->wmp_getattr)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_getattr function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_version = (f_wmp_version) dlsym(context->wmp_handle, "ir_wmp_get_version");
        if (NULL == context->wmp_version)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_get_version function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_itos = (f_wmp_itos) dlsym(context->wmp_handle, "ir_wmp_int_to_str");
        if (NULL == context->wmp_itos)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_int_to_str function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_lltos = (f_wmp_lltos) dlsym(context->wmp_handle, "ir_wmp_longlong_to_str");
        if (NULL == context->wmp_lltos)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_longlong_to_str function. Error: %s\n", dlerror());
            break;
        }

        context->wmp_ftos = (f_wmp_ftos) dlsym(context->wmp_handle, "ir_wmp_float_to_str");
        if (NULL == context->wmp_ftos)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmp_float_to_str function. Error: %s\n", dlerror());
            break;
        }

        result = 0;
    } while(0);

    return result;
}

static void ir_dump_drid_config(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    av_log(ctx, AV_LOG_INFO,
        "Irdeto WM filter configuration:\n"
        "Video property: %s %sx%s:%s@%sfps\n"
        "Profile: %s\n"
        "Scene-cut detection factor: %s\n"
        "DRID: %s\n"
        "Debug banner: %s\n"
        "Logfile prefix: \"%s\", loglevel: %s\n"
        "Statfile prefix: \"%s\", statistics: %s\n"
        "License file: \"%s\"\n"
        "Threads: %s\n"
        "Version: %s\n",
        context->wmp_getattr(context->wmp_ctx, "pixfmt"),
        context->wmp_getattr(context->wmp_ctx, "width"),
        context->wmp_getattr(context->wmp_ctx, "height"),
        context->wmp_getattr(context->wmp_ctx, "linesize"),
        context->wmp_getattr(context->wmp_ctx, "fps"),
        context->wmp_getattr(context->wmp_ctx, "profile"),
        context->wmp_getattr(context->wmp_ctx, "scdfactor"),
        context->drid,
        context->wmp_getattr(context->wmp_ctx, "banner"),
        context->wmp_getattr(context->wmp_ctx, "logfile"),
        context->wmp_getattr(context->wmp_ctx, "loglevel"),
        context->wmp_getattr(context->wmp_ctx, "statfile"),
        context->wmp_getattr(context->wmp_ctx, "statistics"),
        context->wmp_getattr(context->wmp_ctx, "license"),
        context->wmp_getattr(context->wmp_ctx, "threads"),
        context->wmp_version());
}

static void ir_dump_tmid_config(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    av_log(ctx, AV_LOG_INFO,
        "Irdeto WM filter configuration:\n"
        "Video property: %s %sx%s:%s@%sfps\n"
        "Profile: %s\n"
        "Scene-cut detection factor: %s\n"
        "Operator: %s\n"
        "TMID: %s\n"
        "TMID bit interval: %s ms\n"
        "Debug banner: %s\n"
        "Logfile prefix: \"%s\", loglevel: %s\n"
        "Statfile prefix: \"%s\", statistics: %s\n"
        "License file: \"%s\"\n"
        "Threads: %s\n"
        "Version: %s\n",
        context->wmp_getattr(context->wmp_ctx, "pixfmt"),
        context->wmp_getattr(context->wmp_ctx, "width"),
        context->wmp_getattr(context->wmp_ctx, "height"),
        context->wmp_getattr(context->wmp_ctx, "linesize"),
        context->wmp_getattr(context->wmp_ctx, "fps"),
        context->wmp_getattr(context->wmp_ctx, "profile"),
        context->wmp_getattr(context->wmp_ctx, "scdfactor"),
        context->wmp_getattr(context->wmp_ctx, "operator"),
        context->wmp_getattr(context->wmp_ctx, "tmid"),
        context->wmp_getattr(context->wmp_ctx, "wmtime"),
        context->wmp_getattr(context->wmp_ctx, "banner"),
        context->wmp_getattr(context->wmp_ctx, "logfile"),
        context->wmp_getattr(context->wmp_ctx, "loglevel"),
        context->wmp_getattr(context->wmp_ctx, "statfile"),
        context->wmp_getattr(context->wmp_ctx, "statistics"),
        context->wmp_getattr(context->wmp_ctx, "license"),
        context->wmp_getattr(context->wmp_ctx, "threads"),
        context->wmp_version());
}

static void ir_dump_epoch_config(AVFilterContext* const ctx)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    av_log(ctx, AV_LOG_INFO,
        "Irdeto WM filter configuration:\n"
        "Video property: %s %sx%s:%s@%sfps\n"
        "Profile: %s\n"
        "Scene-cut detection factor: %s\n"
        "Operator: %s\n"
        "TMID: %s\n"
        "Epoch-locking mode: %s\n"
        "Initial epoch time: %" PRId64 "\n"
        "Segment duration: %" PRId64 " ms\n"
        "Debug banner: %s\n"
        "Logfile prefix: \"%s\", loglevel: %s\n"
        "Statfile prefix: \"%s\", statistics: %s\n"
        "License file: \"%s\"\n"
        "Threads: %s\n"
        "Version: %s\n",
        context->wmp_getattr(context->wmp_ctx, "pixfmt"),
        context->wmp_getattr(context->wmp_ctx, "width"),
        context->wmp_getattr(context->wmp_ctx, "height"),
        context->wmp_getattr(context->wmp_ctx, "linesize"),
        context->wmp_getattr(context->wmp_ctx, "fps"),
        context->wmp_getattr(context->wmp_ctx, "profile"),
        context->wmp_getattr(context->wmp_ctx, "scdfactor"),
        context->wmp_getattr(context->wmp_ctx, "operator"),
        context->wmp_getattr(context->wmp_ctx, "tmid"),
        (EPOCH_MODE_CUSTOM == context->epoch_mode) ? "custom" : "copy",
        (EPOCH_MODE_CUSTOM == context->epoch_mode) ? context->epoch_time / 1000 : 0,
        (EPOCH_MODE_CUSTOM == context->epoch_mode) ? context->epoch_seglen      : 0,
        context->wmp_getattr(context->wmp_ctx, "banner"),
        context->wmp_getattr(context->wmp_ctx, "logfile"),
        context->wmp_getattr(context->wmp_ctx, "loglevel"),
        context->wmp_getattr(context->wmp_ctx, "statfile"),
        context->wmp_getattr(context->wmp_ctx, "statistics"),
        context->wmp_getattr(context->wmp_ctx, "license"),
        context->wmp_getattr(context->wmp_ctx, "threads"),
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
        context->sequence_id = (uint64_t) -1;

    } while(0);

    return AVERROR(result);
}

static int wm_plugin_postinit(AVFilterLink* const inlink, AVFrame* const frame)
{
    int result = 0;

    do
    {
        AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
        struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

        char  value[64];
        char* plugin_path;
        char* license_file;
        float fps;

        if (VF_STATE_INITIALIZED == context->state)
        {
            break;
        }

        plugin_path = getenv("IR_WM_PLUGIN_PATH");
        if (NULL == plugin_path)
        {
            plugin_path = (char*) IR_WM_PLUGIN_PATH;
        }

        license_file = getenv("IR_LICENSE");
        if (NULL == license_file)
        {
            license_file = (char*) IR_LICENSE;
        }

        context->drid_mode = (strlen(context->drid) > 0) ? 1 : 0;

        context->epoch_time   = -1;
        context->epoch_seglen = -1;
        context->pts_initial  = -1;
        context->pts_last     = -1;
        context->pts_num      =  0;

        if (! strcmp(context->epoch, "off"))
        {
            context->epoch_mode = EPOCH_MODE_OFF;
        }
        else if (! strcmp(context->epoch, "copy"))
        {
            context->epoch_mode = EPOCH_MODE_COPY;
        }
        else
        {
            char* endptr = NULL;

            context->epoch_time = 1000 * strtoll(context->epoch, &endptr, 10);

            if ((endptr && *endptr) || (context->epoch_time < 0))
            {
                av_log(ctx, AV_LOG_ERROR, "Wrong \"epoch\" parameter is specified\n");
                break;
            }

            context->epoch_seglen = (int64_t) context->wmtime;

            if (context->epoch_seglen <= 0)
            {
                av_log(ctx, AV_LOG_ERROR, "Wrong \"wmtime\" parameter is specified\n");
                break;
            }

            // Share the value of Initial Epoch Time
            av_wm_info_set_epoch_time(context->epoch_time / 1000);

            context->epoch_mode = EPOCH_MODE_CUSTOM;
        }

        if (! strcmp(context->sei, "off"))
        {
            context->sei_type = AV_WM_SEI_NONE;
        }
        else if (! strcmp(context->sei, "irdeto_v2"))
        {
            context->sei_type = AV_WM_SEI_IRDETO_V2;
        }
        else if (! strcmp(context->sei, "irdeto"))
        {
            context->sei_type = AV_WM_SEI_IRDETO_V3;
        }
        else if (! strcmp(context->sei, "dash"))
        {
            context->sei_type = AV_WM_SEI_DASH_IF;
        }
        else
        {
            av_log(ctx, AV_LOG_ERROR, "Wrong \"sei\" parameter is specified\n");
            break;
        }

        if ((AV_WM_SEI_NONE != context->sei_type) && (0 != context->drid_mode))
        {
            av_log(ctx, AV_LOG_ERROR, "SEI payload couldn't be provided with DRID\n");
            break;
        }

        if (EPOCH_MODE_OFF != context->epoch_mode)
        {
            if (0 != context->drid_mode)
            {
                av_log(ctx, AV_LOG_ERROR, "Epoch-locking mode couldn't be used with DRID\n");
                break;
            }

            av_log(ctx, AV_LOG_DEBUG, "Irdeto WM filter is started in TMID mode with sequence ID criteria\n");
            result = context->wmp_init(IR_WM_SEQID_UUID, plugin_path, &context->wmp_ctx);
        }
        else if (0 != context->drid_mode)
        {
            av_log(ctx, AV_LOG_DEBUG, "Irdeto WM filter is started in DRID mode\n");
            result = context->wmp_init(IR_WM_DRID_UUID, plugin_path, &context->wmp_ctx);
        }
        else
        {
            av_log(ctx, AV_LOG_DEBUG, "Irdeto WM filter is started in TMID mode\n");
            result = context->wmp_init(IR_WM_TMID_UUID, plugin_path, &context->wmp_ctx);
        }

        if (0 != result)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't init Irdeto WM plugin. Error: %d\n", result);
            break;
        }

        context->wmp_setattr(context->wmp_ctx, "license", license_file);
        context->wmp_setattr(context->wmp_ctx, "threads", context->threads);
        context->wmp_setattr(context->wmp_ctx, "scdfactor", context->scdfactor);

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
        if (0 == context->drid_mode)
        {
            context->wmp_setattr(context->wmp_ctx, "operator", context->oid);
            context->wmp_setattr(context->wmp_ctx, "tmid", context->tmid);
            context->wmp_setattr(context->wmp_ctx, "firstbit", context->firstbit);
            context->wmp_setattr(context->wmp_ctx, "lastbit", context->lastbit);

            if (EPOCH_MODE_OFF == context->epoch_mode)
            {
                ///< Customized plugin attributes
                context->wmp_itos(context->wmtime, value, 64);
                context->wmp_setattr(context->wmp_ctx, "wmtime", value);
            }
        }

        context->wmp_setattr(context->wmp_ctx, "firstframe", context->firstframe);
        context->wmp_setattr(context->wmp_ctx, "lastframe", context->lastframe);
        context->wmp_setattr(context->wmp_ctx, "profile", context->profile);

        ///< Debug attributes, note that loglevel is set to debug on purpose
        context->wmp_setattr(context->wmp_ctx, "banner", context->banner);
        context->wmp_setattr(context->wmp_ctx, "logfile", context->logfile);
        context->wmp_setattr(context->wmp_ctx, "loglevel", "debug");
        context->wmp_setattr(context->wmp_ctx, "statfile", context->statfile);
        context->wmp_setattr(context->wmp_ctx, "statistics", "on");

        if (EPOCH_MODE_OFF != context->epoch_mode)
        {
            ir_dump_epoch_config(ctx);
        }
        else if (0 != context->drid_mode)
        {
            ir_dump_drid_config(ctx);
        }
        else
        {
            ir_dump_tmid_config(ctx);
        }

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

static void get_epoch_time_from_sidedata(AVFilterContext* ctx, AVFrame* const frame, const AVRational frame_rate)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    int64_t time_stamp_ms = -1;

    // Timecode only include information about time, not date. So get the current date value
    uint64_t tc_dd = time(NULL) / 86400;

    // Try to get timecode which conforms to SMPTE ST 12-1
    // Such format of side data is provided by H.264 (AVC) and H.265 (HEVC) decoders
    // The data is an array of 4 uint32_t where the first uint32_t describes how many (1-3) of the other timecodes are used
    AVFrameSideData* side_data = av_frame_get_side_data(frame, AV_FRAME_DATA_S12M_TIMECODE);

    if (side_data)
    {
        const uint32_t* timecodes = (const uint32_t*) side_data->data;

        if ((timecodes) && (*timecodes > 0) && (*timecodes < 4) && ((sizeof(uint32_t) * 4) == side_data->size))
        {
            // Side data can include up to 3 timecodes, take 1st one only
            uint32_t tc_bcd = timecodes[1];

            uint32_t tc_hh = 10 * ((tc_bcd >>  4) & 0x03) + ((tc_bcd      ) & 0x0F); // 6-bit hours
            uint32_t tc_mm = 10 * ((tc_bcd >> 12) & 0x07) + ((tc_bcd >>  8) & 0x0F); // 7-bit minutes
            uint32_t tc_ss = 10 * ((tc_bcd >> 20) & 0x07) + ((tc_bcd >> 16) & 0x0F); // 7-bit seconds
            uint32_t tc_ff = 10 * ((tc_bcd >> 28) & 0x03) + ((tc_bcd >> 24) & 0x0F); // 6-bit frames

            time_stamp_ms = tc_dd * 86400
                          + tc_hh *  3600
                          + tc_mm *    60
                          + tc_ss;
            time_stamp_ms *= 1000;
/*
            if ((frame_rate.num / frame_rate.den) > 30)
                time_stamp_ms += tc_ff * 2000 * frame_rate.den / frame_rate.num;
            else
                time_stamp_ms += tc_ff * 1000 * frame_rate.den / frame_rate.num;
*/
            av_log(ctx, AV_LOG_INFO, "SMPTE ST 12-1 timecode is found: %02u:%02u:%02u:%02u\n",
                  (unsigned int) tc_hh,
                  (unsigned int) tc_mm,
                  (unsigned int) tc_ss,
                  (unsigned int) tc_ff);
        }
        else
        {
            // Something wrong
            av_log(ctx, AV_LOG_ERROR, "Wrong format of AV_FRAME_DATA_S12M_TIMECODE data\n");
        }
    }
/*
    // Try to get GOP timecode in 25 bit timecode format
    // Such format of side data is provided by MPEG-2 decoder
    // Data format is 64-bit integer which is set on the first frame of a GOP (that has a temporal reference of 0)
    side_data = av_frame_get_side_data(frame, AV_FRAME_DATA_GOP_TIMECODE);

    if ((side_data) && (side_data->data) && (sizeof(uint64_t) == side_data->size))
    {
        uint32_t tc_25bit = *((uint64_t*) side_data->data);

        uint32_t tc_hh = (tc_25bit >> 19) & 0x1F; // 5-bit hours
        uint32_t tc_mm = (tc_25bit >> 13) & 0x3F; // 6-bit minutes
        uint32_t tc_ss = (tc_25bit >>  6) & 0x3F; // 6-bit seconds
        uint32_t tc_ff = (tc_25bit      ) & 0x3F; // 6-bit frames

        time_stamp_ms = tc_dd * 86400
                      + tc_hh *  3600
                      + tc_mm *    60
                      + tc_ss;
        time_stamp_ms *= 1000;
    }

    // Try to get timecode from metadata
    // Timecode is a null-terminated C-string in format "HH:MM:SS:FF"
    AVDictionaryEntry* dict_entry = av_dict_get(frame->metadata, "timecode", NULL, 0);

    if (dict_entry)
    {
        AVTimecode tc;

        if (! av_timecode_init_from_string(&tc, frame_rate, dict_entry->value, NULL))
        {
            // Get time stamp value from frame number
            time_stamp_ms = tc_dd * 86400000
                          + tc.start * 1000 / tc.fps;
        }
    }
*/
    if (time_stamp_ms >= 0)
    {
        int64_t seglen = (int64_t) context->wmtime;

        if (seglen > 0)
        {
            context->epoch_time   = time_stamp_ms;
            context->epoch_seglen = seglen;

            av_log(ctx, AV_LOG_INFO, "Initial epoch time: %" PRId64 ", segment duration: %" PRId64 " ms\n",
                   context->epoch_time / 1000,
                   context->epoch_seglen);
        }
        else
        {
            av_log(ctx, AV_LOG_ERROR, "Wrong \"wmtime\" parameter is specified\n");
        }
    }
}

static uint64_t convert_pts_to_msn(AVFilterContext* ctx, const int64_t pts, const AVRational time_base)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    uint64_t msn = 0;

    if ((context->pts_initial < 0) && (context->pts_last < 0))
    {
        // Initial epoch time
        msn = 1 + context->epoch_time / context->epoch_seglen;

        context->pts_initial = pts;
        context->pts_last    = pts;
        context->pts_num     = 0;
    }
    else if (pts >= context->pts_last)
    {
        uint64_t time_stamp_inc = 1000 * (pts - context->pts_initial) * time_base.num / time_base.den;

        msn = 1 + context->epoch_time / context->epoch_seglen + time_stamp_inc / context->epoch_seglen;

        context->pts_last = pts;
        context->pts_num ++;
    }
    else
    {
        uint64_t pts_inc, time_stamp_inc;

        pts_inc = context->pts_last - context->pts_initial;
        if (context->pts_num > 0)
            pts_inc += (context->pts_last - context->pts_initial) / context->pts_num;

        time_stamp_inc = 1000 * pts_inc * time_base.num / time_base.den;

        msn = 1 + context->epoch_time / context->epoch_seglen + time_stamp_inc / context->epoch_seglen;

        // Reset PTS sequence
        context->pts_initial = pts - pts_inc;
        context->pts_last    = pts;
        context->pts_num ++;
    }

    // Convert time stamp to Media Sequence Number
    return msn;
}

static int wm_plugin_process_frame(AVFilterLink* const inlink,
    AVFrame* const frame)
{
    int result = -1;
    AVFilterContext* ctx = (AVFilterContext*) inlink->dst;
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    do
    {
        av_frame_make_writable(frame);

        if (0 != context->drid_mode)
        {
            result = context->wmp_embed(context->wmp_ctx, frame->data[0], "directid", context->drid);
        }
        else
        {
            uint8_t sei_payload[IR_WMP_SEI_PAYLOAD_V3_SIZE];
            size_t payload_size = 0;
            unsigned int  flags = IR_WMP_SEI_FLAG_FIRST_PART | IR_WMP_SEI_FLAG_LAST_PART;

            if (EPOCH_MODE_OFF != context->epoch_mode)
            {
                char value[64];
                uint64_t msn;

                if ((EPOCH_MODE_COPY == context->epoch_mode) && (context->epoch_time < 0 || context->epoch_seglen < 1))
                {
                    // Take the source time stamp and convert it to initial Epoch time
                    get_epoch_time_from_sidedata(ctx, frame, inlink->frame_rate);

                    if (context->epoch_time < 0 || context->epoch_seglen < 1)
                    {
                        av_log(ctx, AV_LOG_WARNING, "Timecode is not found for frame %d, sequenceid = 0 was embedded\n", frame->display_picture_number);
                        result = context->wmp_embed(context->wmp_ctx, frame->data[0], "sequenceid", "0");
                        break;
                    }

                    // Share the value of Initial Epoch Time
                    av_wm_info_set_epoch_time(context->epoch_time / 1000);
                }

                // Convert PTS to Media Sequence Number
                msn = convert_pts_to_msn(ctx, frame->pts, inlink->time_base);
                context->wmp_lltos(msn, value, 64);

                // Process the frame by the Encoder Plugin
                result = context->wmp_embed(context->wmp_ctx, frame->data[0], "sequenceid", value);

                // Generate SEI payload
                if ((AV_WM_SEI_IRDETO_V3 == context->sei_type) && (context->sequence_id != msn) && (result >= 0))
                {
                    payload_size = sizeof(sei_payload);
                    result = context->wmp_sei_payload(context->wmp_ctx, sei_payload, &payload_size, "sequenceid", value, flags);
                    if (result < 0)
                        payload_size = 0;

                    context->sequence_id = msn;
                }
            }
            else
            {
                char value[64];

                // Actually, it is PTS converted to frame display time in ms
                uint64_t pts = 1000 * frame->pts * inlink->time_base.num / inlink->time_base.den;
                uint64_t sequence_id = (context->wmtime > 0) ? (pts / context->wmtime) : context->sequence_id;
                context->wmp_lltos(pts, value, 64);

                // Process the frame by the Encoder Plugin
                result = context->wmp_embed(context->wmp_ctx, frame->data[0], "pts", value);

                // Generate SEI payload
                if ((AV_WM_SEI_IRDETO_V3 == context->sei_type) && (context->sequence_id != sequence_id) && (result >= 0))
                {
                    payload_size = sizeof(sei_payload);
                    result = context->wmp_sei_payload(context->wmp_ctx, sei_payload, &payload_size, "pts", value, flags);
                    if (result < 0)
                        payload_size = 0;

                    context->sequence_id = sequence_id;
                }
            }

            if ((result >= 0) && ((AV_WM_SEI_IRDETO_V2 == context->sei_type) || (AV_WM_SEI_DASH_IF == context->sei_type)))
            {
                // Get bit value and position
                AVIrdetoWatermark ir_wm_info = { 0 };
                ir_wm_info.bitlen      = 0; // Not required for AV_WM_SEI_IRDETO_V2 and AV_WM_SEI_DASH_IF
                ir_wm_info.watermarked = (result != 0) ? 0 : 1;
                av_wm_info_atoi(context->wmp_getattr(context->wmp_ctx, "bitval"), &ir_wm_info.bitval);
                av_wm_info_atoll(context->wmp_getattr(context->wmp_ctx, "bitpos"), &ir_wm_info.bitpos);

                // Generate SEI payload
                if (context->sequence_id != ir_wm_info.bitpos)
                {
                    payload_size = sizeof(sei_payload);
                    result = av_wm_info_alloc_sei(&ir_wm_info, context->sei_type, sei_payload, &payload_size);
                    if (result < 0)
                        payload_size = 0;

                    context->sequence_id = ir_wm_info.bitpos;
                }
            }

            if (payload_size > 0)
            {
                AVFrameSideData* sd = av_frame_new_side_data(frame, AV_FRAME_DATA_IR_SEI_PAYLOAD, payload_size);
                if (sd) memcpy(sd->data, sei_payload, payload_size);
            }
        }

        if (result < 0)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't embed watermark to frame %d. Error: %d\n", frame->display_picture_number, result);
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
//      if (0 != result)
//      {
//          result = 0;
//          break;
//      }

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
        // 8-bit pixel formats
        AV_PIX_FMT_GRAY8,
        AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV422P,
        AV_PIX_FMT_YUV444P,

        // 10-bit pixel formats
        AV_PIX_FMT_GRAY10LE,    // AV_PIX_FMT_GRAY10
        AV_PIX_FMT_YUV420P10LE, // AV_PIX_FMT_YUV420P10
        AV_PIX_FMT_YUV422P10LE, // AV_PIX_FMT_YUV422P10
        AV_PIX_FMT_YUV444P10LE, // AV_PIX_FMT_YUV444P10

        // 12-bit pixel formats
        AV_PIX_FMT_GRAY12LE,    // AV_PIX_FMT_GRAY12
        AV_PIX_FMT_YUV420P12LE, // AV_PIX_FMT_YUV420P12
        AV_PIX_FMT_YUV422P12LE, // AV_PIX_FMT_YUV422P12
        AV_PIX_FMT_YUV444P12LE, // AV_PIX_FMT_YUV444P12

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

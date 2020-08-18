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
#include "vf_det_ctx.h"

/**
* @def          IR_WM_LIB
* @brief        Absolute path to Detection library used by default (production environment)
*/
#define IR_WMDET_LIB "/usr/lib64/irwmd/libirwmd.so"

AVFILTER_DEFINE_CLASS(irdeto_owl_det);


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
        char* plugin_path = getenv("IR_WMDET_LIB");
        if (NULL == plugin_path)
        {
            plugin_path = (char*) IR_WMDET_LIB;
        }

        av_log(ctx, AV_LOG_INFO, "Watermarking plugin %s will be used\n", plugin_path);
        context->handle = dlopen(plugin_path, RTLD_NOW | RTLD_NODELETE);
        if (NULL == context->handle)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load WM plugin %s. Error: %s\n", plugin_path, dlerror());
            break;
        }

        context->wmd_init = (f_wmd_init) dlsym(context->handle, "ir_wmd_init");
        if (NULL == context->wmd_init)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_init function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_extract = (f_wmd_extract) dlsym(context->handle, "ir_wmd_extract");
        if (NULL == context->wmd_extract)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_extract function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_uninit = (f_wmd_uninit) dlsym(context->handle, "ir_wmd_uninit");
        if (NULL == context->wmd_uninit)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_uninit function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_version = (f_wmd_get_version) dlsym(context->handle, "ir_wmd_get_version");
        if (NULL == context->wmd_version)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_get_version function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_reset = (f_wmd_reset) dlsym(context->handle, "ir_wmd_reset");
        if (NULL == context->wmd_reset)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_reset function. Error: %s\n", dlerror());
            break;
        }

        context->wmd_set_pc = (f_wmd_set_presence_check) dlsym(context->handle, "ir_wmd_set_presence_check");
        if (NULL == context->wmd_set_pc)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't load ir_wmd_set_presence_check function. Error: %s\n", dlerror());
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
        "Irdeto %s configuration:\nVersion: %s\nNodes: %" PRId64 "\nOperators: %" PRId64 "\nSummary: %s\nMode: %s\n",
        ctx->filter->name, context->wmd_version(), context->nodes, context->operator, context->summary, context->mode);
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
    int i = 0;

    do
    {
        if (0 != context->operator && 0 != context->owlo_type)
        {
            av_log(ctx, AV_LOG_ERROR, "operator and owlo_type can't be set at the same time.\n");
            result = EINVAL;
            break;
        }

        if (0 != context->owlo_type && context->owlo_type < 8)
        {
            av_log(ctx, AV_LOG_ERROR, "owlo_type must be in range of 8 to 15 inclusive.\n");
            result = EINVAL;
            break;
        }

        if (strcmp(context->mode, "fast") != 0 && strcmp(context->mode, "exhausted") != 0)
        {
            av_log(ctx, AV_LOG_ERROR, "mode must be either fast or exhausted.\n");
            result = EINVAL;
            break;
        }

        // Load plugin from file to memory
        if (0 != ir_load_wm_plugin(ctx))
        {
            result = ENOENT;
            break;
        }
        context->tmid_size = 2048;

        // Initialize operator output
        for (i = 0; i < 1024; i++)
        {
            context->det_report[i].detected = 0;
            context->det_report[i].all_symbols = 0;
            context->det_report[i].unique_symbols = 0;
            context->det_report[i].colluded = 0;
            memset(context->det_report[i].res, '?', context->tmid_size);
            context->det_report[i].res[context->tmid_size] = '\0';
        }

        context->owlo_report.unique_device_ids = 0;
        context->owlo_report.num_detections = 0;
        context->owlo_report.size = 100;
        context->owlo_report.device_id_list = (uint32_t*) malloc(context->owlo_report.size * sizeof(uint32_t));
        memset(context->owlo_report.device_id_list, 0, context->owlo_report.size * sizeof(uint32_t));

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
        // In case filter is initialized imediately exit and continue processing
        if (VF_STATE_INITIALIZED == context->state)
        {
            break;
        }
        // Print configuration only once and set flag that filter and library now initialized
        ir_dump_config(ctx);

        context->wmd_ctx = context->wmd_init(context->nodes, frame->width, frame->height, frame->linesize[0]);
        if (NULL == context->wmd_ctx)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't init Irdeto WM detection plugin.\n");
            result = -1;
            break;
        }

        if (strcmp(context->mode, "exhausted") == 0)
        {
            ///< Disable presence check
            context->wmd_set_pc(context->wmd_ctx, 0);
        }
        else
        {
            ///< Enable presence check
            context->wmd_set_pc(context->wmd_ctx, 1);
        }

        float fps = inlink->frame_rate.num / (float) inlink->frame_rate.den;
        context->s_pts          = frame->pts * av_q2d(inlink->time_base);
        context->frame_count    = context->s_pts / fps;

        context->state = VF_STATE_INITIALIZED;

    } while(0);

    return result;
}

static void handle_tmid(AVFilterContext* const ctx, uint64_t payload)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    uint32_t op = payload & 0x00000000000003ff;
    uint32_t pos = (payload & 0x00000007ffffffff) >> 10;
    uint32_t value = (payload & 0x0000000800000000) >> 35;

    ///< check if detected operator id matches specified one
    int match = (context->operator == 0 || op == context->operator);

    ///< if result is not desired, ignore the result and reset OWL detector hints
    if (!match || pos >= context->tmid_size || context->owlo_type != 0)
    {
        context->wmd_reset(context->wmd_ctx);
        av_log(ctx, AV_LOG_DEBUG, "Reset hints, false positive: %09" PRIx64 "\n", payload);
        return;
    }

    av_log(ctx, AV_LOG_INFO, "[Detected: FN: %.16llu, WM: %.8x, Symbol: %u, Position: %.4u, Operator: %.4u]\n",
        (long long unsigned)context->frame_count, (pos << 1) | value, value, pos, op);

    context->det_report[op].detected = 1;
    context->det_report[op].all_symbols += 1;

    if (context->det_report[op].res[context->tmid_size - 1 - pos] == '?')
    {
        context->det_report[op].res[context->tmid_size - 1 - pos] = (value == 1) ? '1' : '0';
        context->det_report[op].unique_symbols += 1;
    }
    else
    {
        char s = (value == 1) ? '1' : '0';
        context->det_report[op].res[context->tmid_size - 1 - pos] =
            (s == context->det_report[op].res[context->tmid_size - 1 - pos]) ? s : '!';
        context->det_report[op].colluded +=
            (context->det_report[op].res[context->tmid_size - 1 - pos] == '!') ? 1 : 0;
    }
}

static void handle_direct_id(AVFilterContext* const ctx, uint64_t payload)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;
    uint8_t overlay_type_id = ((uint64_t)payload & 0x00000000000003c0) >> 6;
    uint32_t device_id = (((uint64_t)payload & 0x000000000000003f) << 26) | (((uint64_t)payload & 0x0000000ffffffc00) >> 10);

    int match = (context->owlo_type == 0 || context->owlo_type == overlay_type_id);
    if (!match || context->operator != 0)
    {
        context->wmd_reset(context->wmd_ctx);
        av_log(ctx, AV_LOG_DEBUG, "Reset hints, false positive: %09" PRIx64 "\n", payload);
        return;
    }

    av_log(ctx, AV_LOG_INFO, "[Detected: FN: %.16llu, WM: %09" PRIx64 ", Overlay Type: %u, Device ID: %u]\n",
        (long long unsigned)context->frame_count, payload, overlay_type_id, device_id);
    context->owlo_report.num_detections++;
    int k = 0;
    /* Update device ID detected and save it if it's unique */
    for (k = 0; k < context->owlo_report.unique_device_ids; k++)
    {
        if (context->owlo_report.device_id_list[k] == device_id)
        {
            break;
        }
    }
    if (k == context->owlo_report.unique_device_ids)
    {
        context->owlo_report.device_id_list[context->owlo_report.unique_device_ids++] = device_id;
        if (context->owlo_report.unique_device_ids == context->owlo_report.size)
        {
            context->owlo_report.size += 100;
            context->owlo_report.device_id_list = (uint32_t*) realloc(context->owlo_report.device_id_list,
                context->owlo_report.size * sizeof(uint32_t));
        }
    }
}

/**
* @brief        Filter single frame througn detection pipeline
* @param        [in] ctx        AVFilter context
* @param        [in] r          Pointer to the list of detection results
* @param        [in] number     Number of detection results
*/
static void wm_process_payload(AVFilterContext* const ctx, uint64_t payload)
{
    struct ir_pf_context* context = (struct ir_pf_context*) ctx->priv;

    /**
    * @warning      0 is NOT a valid payload, in case of IR_WMD_STATUS_DETECTED and payload is 0, it's NOT
    *               a valid detection, so hints must be reset.
    */
    if (0 == payload)
    {
        context->wmd_reset(context->wmd_ctx);
        av_log(ctx, AV_LOG_DEBUG, "Reset hints, false positive: %09" PRIx64 "\n", payload);
        return;
    }

    uint32_t op = payload & 0x00000000000003ff;

    /**
    * @note     Operator ID [1, 511] and bit position [0, 2047] is headend WM, others are OVERLAY WM
    * @note     In case of OVERLAY WM, we just unpack 36bit payload into 4bit overlay type and 32bit device id
    * @ref      https://irdeto.atlassian.net/wiki/spaces/CSMUW/pages/1440024040/OWL+Identities+-+Split+Between+Head-end+and+Client
    * @warning  It's known that repack wm_symbol/wm_pos/operator_id again into 36bit payload and unpack it
    *           again with different interpretation. I hope later we will update detector plugin to avoid this
    */
    if (op >= 512)
    {
        handle_direct_id(ctx, payload);
    }
    else
    {
        handle_tmid(ctx, payload);
    }
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

    do
    {
        // Exit immediately if initialized
        if (VF_STATE_UNINITIALIZED == context->state)
        {
            break;
        }

        // Extract symbol from frame
        uint64_t payload = 0;
        int det_result = context->wmd_extract(context->wmd_ctx, frame->data[0], &payload);
        if (IR_WMD_STATUS_OK != det_result && IR_WMD_STATUS_DETECTED != det_result)
        {
            av_log(ctx, AV_LOG_ERROR, "Can't process frame %d. Error: %d\n", context->frame_count, det_result);
            result = -1;
            break;
        }
        if (IR_WMD_STATUS_DETECTED == det_result)
        {
            wm_process_payload(ctx, payload);
        }

        result = 0;
    } while(0);

    context->frame_count++;
    if (context->prog_flag && 0 == (context->frame_count % 50)) av_log(ctx, AV_LOG_INFO, "Frame number processed: %llu !\n", context->frame_count);
    context->f_pts = frame->pts * av_q2d(inlink->time_base); // Seconds
    return result;
}

/**
* @brief        Filter single frame througn detection pipeline
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

    context->frame_count++;

    context->wmd_uninit(context->wmd_ctx);
    if (context->handle)
    {
        dlclose(context->handle);
    }

    if (0 == strncmp("on", context->summary, 2))
    {
        av_log(ctx, AV_LOG_INFO, "Detection summary:\n");
        for (int i = 0; i < 1024; i++)
        {
            if (1 != context->det_report[i].detected)
            {
                continue;
            }

            av_log(ctx, AV_LOG_INFO, "Operator: %.4u [%s] (S:%d, U:%d, C:%d)\n",
                i, context->det_report[i].res, context->det_report[i].all_symbols,
                context->det_report[i].unique_symbols, context->det_report[i].colluded);
        }

        if (context->owlo_report.num_detections)
        {
            av_log(ctx, AV_LOG_INFO, "Overlay Detections: %d. Number of Unique Device: %d\n",
                context->owlo_report.num_detections, context->owlo_report.unique_device_ids);
            for (int i = 0; i < context->owlo_report.unique_device_ids; i++)
            {
                av_log(ctx, AV_LOG_INFO, "Device ID: %u\n", context->owlo_report.device_id_list[i]);
            }
        }
    }

    free(context->owlo_report.device_id_list);

    int s_min   = context->s_pts / 60;
    int s_hours = s_min / 60;
    s_min = s_min - (s_hours * 60);
    int s_sec   = context->s_pts - (((s_hours * 60) + s_min) * 60);

    int f_min   = context->f_pts / 60;
    int f_hours = f_min / 60;
    f_min = f_min - (f_hours * 60);
    int f_sec   = context->f_pts - (((f_hours * 60) + f_min) * 60);

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

AVFilter ff_vf_irdeto_owl_det =
{
    .name            = "irdeto_owl_det",
    .description     = NULL_IF_CONFIG_SMALL("Irdeto OWL Watermark detection plugin"),
    .priv_size       = sizeof(struct ir_pf_context),
    .priv_class      = &irdeto_owl_det_class,
    .init            = wm_plugin_init,
    .uninit          = wm_plugin_uninit,
    .query_formats   = wm_plugin_register_formats,
    .inputs          = wm_plugin_inputs,
    .outputs         = wm_plugin_outputs,
    .process_command = NULL,
};

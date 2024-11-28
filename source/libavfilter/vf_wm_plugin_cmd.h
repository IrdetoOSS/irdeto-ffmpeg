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

#ifndef _VF_WN_PLUGIN_CMD_H_
#define _VF_WN_PLUGIN_CMD_H_

// FFmpeg libavutil includes
#include "libavutil/opt.h"

// Irdeto AVFilter context include
#include "vf_wm_plugin_ctx.h"

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
* @brief        Irdeto FB2 watermark video filter command line options
* @note         FFmpeg video filter pipeline allows to directly pass command
*               line argument into context structure, but for other use case
*               of the plugin, please pass these parameters properly
********************************************************************************
*/
static const AVOption irdeto_owl_emb_options[] =
{
    {
        "wmthreads",
        "Number of threads used by watermarking library",
        IR_CMD_OFFSET(threads),
        AV_OPT_TYPE_STRING,
        {
            .str = "4"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "scdfactor",
        "Factor parameter for scene-cut detection module, it is used to tune sensitivity of detection",
        IR_CMD_OFFSET(scdfactor),
        AV_OPT_TYPE_STRING,
        {
            .str = "4.0"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "firstbit",
        "Starting bit position (included) for the watermark embedding",
        IR_CMD_OFFSET(firstbit),
        AV_OPT_TYPE_STRING,
        {
            .str = "0"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "lastbit",
        "Ending bit position (included) for the watermark embedding, by default lastbit is length of input TMID - 1",
        IR_CMD_OFFSET(lastbit),
        AV_OPT_TYPE_STRING,
        {
            .str = "-1"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "firstframe",
        "First frame of the decoded video stream to be watermarked. "
        "All frames in [firstframe, lastframe] will be watermarked",
        IR_CMD_OFFSET(firstframe),
        AV_OPT_TYPE_STRING,
        {
            .str = "0"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "lastframe",
        "Last frame of the decoded video stream to be watermarked. "
        "All frames in [firstframe, lastframe] will be watermarked",
        IR_CMD_OFFSET(lastframe),
        AV_OPT_TYPE_STRING,
        {
            .str = "0"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "operator",
        "Operator ID to be used for the embedding",
        IR_CMD_OFFSET(oid),
        AV_OPT_TYPE_STRING,
        {
            .str = "511"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "tmid",
        "Identifier to be embedded over time in the video stream",
        IR_CMD_OFFSET(tmid),
        AV_OPT_TYPE_STRING,
        {
            ///< TMID becomes 1 0 switch all the time
            .str = "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "drid",
        "32-bit unique identifier (device ID, uploader ID, etc) "
        "which is used instead of TMID to be embedded in the video stream",
        IR_CMD_OFFSET(drid),
        AV_OPT_TYPE_STRING,
        {
            .str = ""
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "wmtime",
        "Period of one watermark symbol (only used by general/default plugin) "
        "it will be ignored if plugin used is not general/default",
        IR_CMD_OFFSET(wmtime),
        AV_OPT_TYPE_UINT64,
        {
            .i64 = 10000
        },
        0,
        UINT32_MAX,
        IR_CMD_FLAGS
    },
    {
        "epoch",
        "Epoch-locking mode (and initial epoch time in some cases). By default, "
        "set to \"off\" (disabled). Set it to \"copy\" to take epoch time value "
        "from the source, or specify initial epoch time (in seconds)",
        IR_CMD_OFFSET(epoch),
        AV_OPT_TYPE_STRING,
        {
            .str = "off"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "profile",
        "Profile of encoder plugin, for details please read user guide",
        IR_CMD_OFFSET(profile),
        AV_OPT_TYPE_STRING,
        {
            .str = "default"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "sei",
        "Type of SEI payload (off, irdeto, irdeto_v2, dash). "
        "If specified, SEI payload will be additionally provided to an encoder with the frame data.",
        IR_CMD_OFFSET(sei),
        AV_OPT_TYPE_STRING,
        {
            .str = "off"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "banner",
        "If set to on, visual debug banner will also be printed to the "
        "watermarked frames. By default it is switched off.",
        IR_CMD_OFFSET(banner),
        AV_OPT_TYPE_STRING,
        {
            .str = "off"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "logfile",
        "Log file path to store output logs, by default it is not specified and logs are disabled",
        IR_CMD_OFFSET(logfile),
        AV_OPT_TYPE_STRING,
        {
            .str = ""
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "statfile",
        "File path to store embedding statistics, by default it is not specified and statistics is not collected",
        IR_CMD_OFFSET(statfile),
        AV_OPT_TYPE_STRING,
        {
            .str = ""
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    { NULL }
};

#endif  /* !_VF_WN_PLUGIN_CMD_H_ */

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

#ifndef _VF_DET_CTX_H_
#define _VF_DET_CTX_H_

// FFmpeg libavutil includes
#include "libavutil/opt.h"

#include <stdint.h>

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
* @struct       final_res
* @brief        Structure to handle end results when summary is printed
*/
struct final_res
{
    int     detected;
    int     all_symbols;
    int     unique_symbols;
    int     colluded;
    char    res[2049];
};

struct overlay_res
{
    int     unique_device_ids;
    int     num_detections;
    uint32_t* device_id_list;
    size_t  size;
};


/**
* @brief    OK, no error
*/
#define IR_WMD_STATUS_OK 0

/**
* @brief    Watermark is detected from input frame
*/
#define IR_WMD_STATUS_DETECTED 1

/**
* @brief    Failed to detect, error occurred in OWL detector library
*/
#define IR_WMD_STATUS_FAIL -1

/**
* @brief    Function or feature is not implemented in current version.
*/
#define IR_WMD_STATUS_NI -2

/**
* @brief    Invalid argument passed, for example, invalid data, NULL pointer, etc.
*/
#define IR_WMD_STATUS_BADARG -3

#define IR_WMD_OPERATOR_MAX 1023

/**
* @typedef      ir_wmd_context
* @brief        Watermark Detection context declaration
*/
typedef void* ir_wmd_context;

/**
* @typedef      f_wmd_init
* @brief        Definition of runtime loading symbol for ir_wmd_init function
* @see          Look at ir_wmd_init for more information
* @return       IR_WMD_STATUS_OK on success, error code - otherwise
*/
typedef ir_wmd_context (*f_wmd_init)(uint32_t threads, size_t width, size_t height, size_t linesize);

/**
* @typedef      ir_wmd_extract
* @brief        Definition of runtime loading symbol for ir_wmd_extract function
* @see          Look at ir_wmd_extract for more information
* @return       4 if WM symbol was detected, 0 in case symbol was not
*               detected and no error occurred, error code - otherwise
*/
typedef int (*f_wmd_extract)(ir_wmd_context ctx, const void* const frame, uint64_t* const payload);

/**
* @typedef      f_wmd_uninit
* @brief        Definition of runtime loading symbol for ir_wmd_uninit function
* @see          Look at ir_wmd_uninit for more information
* @return       void
*/
typedef void (*f_wmd_uninit)(ir_wmd_context ctx);

/**
* @see          Look at ir_wmd_get_version for more information
* @return       C-style ASCII string with version information on success,
*               NULL pointer - otherwise
*/
typedef char* (*f_wmd_get_version)(void);

/**
* @typedef      f_wmd_reset
* @brief        Definition of runtime loading symbol for ir_wmd_reset function
* @see          Look at ir_wmd_reset for more information
* @return       void
*/
typedef void (*f_wmd_reset)(ir_wmd_context ctx);

/**
* @typedef      f_wmd_set_presence_check
* @brief        Definition of runtime loading symbol for ir_wmd_set_presence_check function
* @see          Look at ir_wmd_set_presence_check for more information
* @return       void
*/
typedef void (*f_wmd_set_presence_check)(ir_wmd_context ctx, int enable);

/**
* @struct       ir_pf_context
* @brief        Irdeto Video Filter context structure
*/
typedef struct ir_pf_context
{
    const AVClass* class;   ///< FFmpeg class pointer

    void*               handle;         ///< Handle for dlopen (don't be as stupid as myself)

    ir_wmd_context      wmd_ctx;        ///< WM detection plugin context
    f_wmd_init          wmd_init;       ///< Initialization routine
    f_wmd_uninit        wmd_uninit;     ///< Uninit
    f_wmd_extract       wmd_extract;    ///< Extract symbol
    f_wmd_get_version   wmd_version;    ///< Report a version
    f_wmd_reset         wmd_reset;      ///< Reset hints
    f_wmd_set_presence_check wmd_set_pc;///< Set presence check

    float               s_pts;
    float               f_pts;

    uint64_t            frame_count;

    int                 tmid_size;

    int64_t             nodes;
    int64_t             operator;
    int64_t             owlo_type;
    const char*         summary;
    const char*         progress;   ///< Flag which indicate whenver we need to put progress
    const char*         mode;       ///< string mode, either fast or exhausted
    struct final_res    det_report[1024];
    struct overlay_res  owlo_report;

    int prog_flag;                  ///< Integer progress flag will be checked on every frame

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
static const AVOption irdeto_owl_det_options[] =
{
    {
        "nodes",
        "Number of nodes used for computations",
        IR_CMD_OFFSET(nodes),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 4
        },
        1,
        128,
        IR_CMD_FLAGS
    },
    {
        "operator",
        "Operator ID in range [1, 511], if set detector will only report watermark with matching operator ID, "
        "by default detector will report all watermark",
        IR_CMD_OFFSET(operator),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0
        },
        0,
        511,
        IR_CMD_FLAGS
    },
    {
        "owlo_type",
        "Overlay type ID in range [8, 15], if set detector will only report watermark with matching type ID, "
        "by default detector will report all watermark",
        IR_CMD_OFFSET(owlo_type),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0
        },
        0,
        15,
        IR_CMD_FLAGS
    },
    {
        "summary",
        "Flag which indicates summary per operator. For example: on or off",
        IR_CMD_OFFSET(summary),
        AV_OPT_TYPE_STRING,
        {
            .str = "on"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "progress",
        "Flag which indicates that filter will print progress. For example: on or off",
        IR_CMD_OFFSET(progress),
        AV_OPT_TYPE_STRING,
        {
            .str = "off"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    {
        "mode",
        "Specify under which mode detector will be running. Allowed values: fast or exhausted. Default is fast",
        IR_CMD_OFFSET(mode),
        AV_OPT_TYPE_STRING,
        {
            .str = "fast"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    { NULL }
};

#endif /* !_VF_DET_CTX_H_ */

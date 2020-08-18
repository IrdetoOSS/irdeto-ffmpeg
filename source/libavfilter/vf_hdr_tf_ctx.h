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

#ifndef _VF_HDR_TF_CTX_H_
#define _VF_HDR_TF_CTX_H_

// FFmpeg libavutil includes
#include "libavutil/opt.h"

#define VF_OTT_VERSION_MAJOR    0
#define VF_OTT_VERSION_MIDDLE   0
#define VF_OTT_VERSION_MINOR    9
#define VF_OTT_RELEASE          0

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

typedef struct ir_hdr_config
{
    int tf_type;
    int norm_base;
};

/**
********************************************************************************
* @struct       ir_pf_context
* @brief        Irdeto Video Filter context structure
********************************************************************************
*/
typedef struct ir_hdr_context
{
    const AVClass*  class;      ///< FFmpeg class pointer

    struct ir_hdr_config config;

    size_t  width;
    size_t  height;
    size_t  linesize;
    uint8_t* output_buffer;

    /**
    ****************************************************************************
    * @brief    Indication of the WM plugin state in current moment of time
    ****************************************************************************
    */
    VF_STATE state;
} IrdetoHDRContext;

/**
********************************************************************************
* @def      IR_CMD_OFFSET
* @brief    List of options for the filter set through the var=value syntax by
*           the offset of the structure
********************************************************************************
*/
#define IR_CMD_OFFSET(x) offsetof(struct ir_hdr_context, x)

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
********************************************************************************
*/
static const AVOption irhdrtf_options[] =
{
    {
        "tf_type",
        "Type of transfer function, 0 means EOTF, 1 means OETF",
        IR_CMD_OFFSET(config.tf_type),
        AV_OPT_TYPE_INT,
        {
            .i64 = 0
        },
        0,
        1,
        IR_CMD_FLAGS
    },
    {
        "norm_base",
        "Value of basis of normalization applied during transfer function",
        IR_CMD_OFFSET(config.norm_base),
        AV_OPT_TYPE_INT,
        {
            .i64 = 1023
        },
        1,
        65535,
        IR_CMD_FLAGS
    },
    { NULL }
};


#endif /* !_VF_HDR_TF_CTX_H_ */

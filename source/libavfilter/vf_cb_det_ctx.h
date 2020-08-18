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

#ifndef _VF_CB_DET_CTX_H_
#define _VF_CB_DET_CTX_H_

#include <stdint.h>
#include <stddef.h>

// FFmpeg libavutil includes
#include "libavutil/opt.h"

/**
********************************************************************************
* @enum         VF_STATE
* @brief        Video filter states representation
********************************************************************************
*/
typedef enum
{
    VF_STATE_UNINITIALIZED     = 0,    ///< Video filter uninitialized
    VF_STATE_INITIALIZED       = 1     ///< Video filter initialized
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
    char    res[257];
};

/**
********************************************************************************
* @enum         CBWM_STATUS
* @brief        CBWM internal error codes will be mapped to external API as
*               integers
********************************************************************************
*/
typedef enum
{
    CBWM_STATUS_OK             = 0,    ///< No error happened
    CBWM_STATUS_NI             = 1,    ///< Function/Feature not implemented
    CBWM_STATUS_BADARG         = 2,    ///< Invalid/Wrong input passed
    CBWM_STATUS_FFTFAIL        = 3,    ///< FFTw library execution failed
    CBWM_STATUS_DETECTED       = 4,    ///< Watermark was detected
    CBWM_STATUS_SYNC           = 5,    ///< Watermark can't be detected due to attack

    /**
    ****************************************************************************
    * @brief    Can't embed watermark since library was not properly configured.
    *           Most likely it's related to passing pixmft data cause other
    *           functions has default behavior.
    ****************************************************************************
    */
    CBWM_STATUS_BADCONF        = 6,
    CBWM_STATUS_BADPF          = 7,
    CBWM_STATUS_AVG_IN_PROCESS = 8,
    CBWM_STATUS_FAIL           = -1,   ///< Generic error happened
} CBWM_STATUS;

/**
********************************************************************************
* @enum         IR_WMD_PF
* @brief        Pixel formats supported by WM detection plugin
********************************************************************************
*/
typedef enum
{
    IR_WMD_PF_YUV400           = 0,
    IR_WMD_PF_YUV420           = 1,
    IR_WMD_PF_YUV422           = 2,
    IR_WMD_PF_YUV420_10BIT     = 3,
    IR_WMD_PF_YUV422_10BIT     = 4,

    /**
    ****************************************************************************
    * @warning  Watermarking library officially doesn't support YUV444 chroma
    *           subsampling mode. Since embedding done only to Y plane the
    *           following workaround is applied
    ****************************************************************************
    */
    IR_WMD_PF_YUV444 = IR_WMD_PF_YUV422,
    IR_WMD_PF_YUV444_10BIT = IR_WMD_PF_YUV422_10BIT

} IR_WMD_PF;

/**
********************************************************************************
* @enum         IR_WMD_MODE
* @brief        WM detection mode
********************************************************************************
*/
typedef enum
{
    /**
    ****************************************************************************
    * @brief    Simple processing mode takes on input set of operators to search
    *           and after first symbol will be detected it will ignore all
    *           other operators except currently detected
    ****************************************************************************
    */
    IR_WMD_MODE_SIMPLE         = 0,

    /**
    ****************************************************************************
    * @brief    Drops detection history on every frame and searches for all
    *           possible combinations
    * @warning  This is experimental mode where results has to be proven by
    *           more tests. As a use-case can be used as wildcard option to find
    *           all possible options (even false positive)
    ****************************************************************************
    */
    IR_WMD_MODE_EXHAUSTIVE     = 1,
    IR_WMD_MODE_MAX            = IR_WMD_MODE_EXHAUSTIVE
} IR_WMD_MODE;

/**
********************************************************************************
* @struct       ir_cbwm_flags_t
* @brief        Configuration flags for CBWM detector.
********************************************************************************
*/
typedef struct {
    uint32_t big_endian : 1;
    uint32_t invert     : 1;
    uint32_t reserved   : 30;
} ir_cbwm_flags_t;

/**
********************************************************************************
* @struct       ir_wmd_config
* @brief        Configuration structure for Watermark detection context
********************************************************************************
*/
struct ir_wmd_config
{
    IR_WMD_MODE     mode;       ///< Detection mode
    size_t          cores;      ///< Number of cores will be used for detection
    size_t          width;      ///< Frame width in pixels
    size_t          height;     ///< Frame height in pixels
    size_t          linesize;   ///< Number of bytes per single line
    float           fps;        ///< Frame rate of the video in range [15.0..60.0]
    size_t          fpi;        ///< Number of frames per detection interval
    IR_WMD_PF       pf;         ///< Pixel format of the
    ir_cbwm_flags_t flags;      ///< Scheme releated flags
};

/**
********************************************************************************
* @typedef      ir_cbwmd_t
* @brief        Public context/handle definition of Irdeto CBWM detection library
* @warning      Implementation of a context is not threadsafe
* @note         This context MUST be passed to each an every function of the API
********************************************************************************
*/
typedef void* ir_cbwmd_t;

/**
********************************************************************************
* @brief        Initialize CBWM detection plugin library
* @param        [in] ctx    Watermark detection context
* @return       void
********************************************************************************
*/
typedef int (*f_cbwmd_init)(const struct ir_wmd_config* const conf, ir_cbwmd_t ctx);

/**
********************************************************************************
* @brief        Uninitialize CBWM detection plugin library
* @param        [in] ctx    Watermark detection context
* @return       void
********************************************************************************
*/
typedef void (*f_cbwmd_uninit)(ir_cbwmd_t ctx);

/**
********************************************************************************
* @brief        Extract watermark payload from Luma buffer of an image
* @param        [in] ctx    Valid CBWM detection context pointer
* @param        [in] image  Image Y buffer as planar structure
* @param        [out] payload   32 bits of WM payload (LSB)
* @return       0 for success, negative error code - otherwise
********************************************************************************
*/
typedef int (*f_cbwmd_extract)(ir_cbwmd_t ctx, void* image, int *detected,
    uint32_t* cssn);

/**
********************************************************************************
* @brief        Provide version information of the Irdeto CBWM Watermark
*               detection library
* @return       Version information as C-string
********************************************************************************
*/
typedef char* (*f_cbwmd_get_version)(void);

/**
*******************************************************************************
* @struct       ir_pf_context
* @brief        Irdeto Video Filter context structure
********************************************************************************
*/
typedef struct ir_pf_context
{
    const AVClass* class;   ///< FFmpeg class pointer

    void*               handle;         ///< Handle for dlopen (don't be as stupid as myself)

    ir_cbwmd_t          wmd_ctx;        ///< WM detection plugin context
    f_cbwmd_init        wmd_init;       ///< Initialization routine
    f_cbwmd_uninit      wmd_uninit;     ///< Uninit
    f_cbwmd_extract     wmd_extract;    ///< Extract symbol
    f_cbwmd_get_version wmd_version;    ///< Report a version

    float               s_pts;
    float               f_pts;
    
    uint64_t            frame_count;
    int                 type;
    
    int64_t             mode;
    int64_t             nodes;
    char*               ops;
    int64_t             fpi;
    const char*         summary;
    const char*         progress;   ///< Flag which indicate whenver we need to put progress

    int                 prog_flag;  ///< Integer progress flag will be checked on every frame

    const char*         *endian;    /// CBWM -> Endianness
    int64_t             invert;     /// CBWM -> Inversion
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
static const AVOption irdeto_cbwm_det_options[] =
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
        "fpi",
        "Number of frames per detection interval",
        IR_CMD_OFFSET(fpi),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 24
        },
        1,
        UINT32_MAX,
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
        "invert",
        "Flag which indicates if there is a need to flip bits of the extracted payload data. For example: 0 or 1",
        IR_CMD_OFFSET(invert),
        AV_OPT_TYPE_INT64,
        {
            .i64 = 0
        },
        0,
        1,
        IR_CMD_FLAGS
    },
    {
        "endian",
        "Flag which sets endiannes of the extracted CSSN number. For example: le or be",
        IR_CMD_OFFSET(endian),
        AV_OPT_TYPE_STRING,
        {
            .str = "le"
        },
        CHAR_MIN,
        CHAR_MAX,
        IR_CMD_FLAGS
    },
    { NULL }
};

#endif /* !_VF_CB_DET_CTX_H_ */

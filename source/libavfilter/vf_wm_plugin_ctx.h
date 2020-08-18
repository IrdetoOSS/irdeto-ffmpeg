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

#ifndef _VF_OTT_CTX_H_
#define _VF_OTT_CTX_H_

#include <stdint.h>

#define VF_WMP_VERSION_MAJOR    2
#define VF_WMP_VERSION_MIDDLE   0
#define VF_WMP_VERSION_MINOR    0
#define VF_WMP_RELEASE          0

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
* @typedef      ir_wmp_context
* @brief        Definition of WM plugin context
* @note         This context used to do bookkeeping of WM parameters passed. Multiple contexts can be used in the
*               same process. Implementation of the library is thread-safe and all calls are re-entrant.
*/
typedef void* ir_wmp_context;

/**
* @typedef      f_wmp_init
* @brief        Function type of ir_wmp_init function
* @see          Look into ir_wmp_init documentation for details
*/
typedef int (*f_wmp_init)(const char* const uuid, const char* const path, ir_wmp_context* ctx);

/**
* @typedef      f_wmp_setattr
* @brief        Function type of ir_wmp_setattr function
* @see          Look into ir_wmp_setattr documentation for details
*/
typedef int (*f_wmp_setattr)(ir_wmp_context ctx, const char* const attr, const char* const value);

/**
* @typedef      f_wmp_getattr
* @brief        Function type of ir_wmp_getattr function
* @see          Look into ir_wmp_getattr documentation for details
*/
typedef const char* (*f_wmp_getattr)(ir_wmp_context ctx, const char* const attr);

/**
* @typedef      f_wmp_configure
* @brief        Function type of ir_wmp_configure function
* @see          Look into ir_wmp_configure documentation for details
*/
typedef int (*f_wmp_configure)(ir_wmp_context ctx) __attribute__((warn_unused_result));

/**
* @typedef      f_wmp_embed
* @brief        Function type of ir_wmp_embed function
* @see          Look into ir_wmp_embed documentation for details
*/
typedef int (*f_wmp_embed)(ir_wmp_context ctx, void* const frame, const char* const attr, const char* const value)
    __attribute__((warn_unused_result));

/**
* @typedef      f_wmp_uninit
* @brief        Function type of ir_wmp_uninit function
* @see          Look into ir_wmp_uninit documentation for details
*/
typedef void (*f_wmp_uninit)(ir_wmp_context ctx);

/**
* @typedef      f_wmp_version
* @brief        Function type of ir_wmp_get_version function
* @see          Look into ir_wmp_get_version documentation for details
*/
typedef char* (*f_wmp_version)(void);

/**
* @typedef      f_wmp_itos
* @brief        Function type of ir_wmp_int_to_str function
* @see          Look into ir_wmp_int_to_str documentation for details
*/
typedef int (*f_wmp_itos)(int value, char* const strval, size_t size);

/**
* @typedef      f_wmp_ftos
* @brief        Function type of ir_wmp_float_to_str function
* @see          Look into ir_wmp_float_to_str documentation for details
*/
typedef int (*f_wmp_ftos)(float value, char* const strval, size_t size);

/**
* @typedef      f_wmp_stot
* @brief        Function type of ir_wmp_symbol_to_tmid function
* @see          Look into ir_wmp_symbol_to_tmid documentation for details
*/
typedef int (*f_wmp_stot)(char symbol, char* const tmid_str, size_t length);

/**
********************************************************************************
* @struct       ir_pf_context
* @brief        Irdeto Video Filter context structure
********************************************************************************
*/
typedef struct ir_pf_context
{
    const AVClass* class;   ///< FFmpeg class pointer

    void*               wmp_handle;     ///< WM plugin handle
    ir_wmp_context      wmp_ctx;        ///< WM plugin context
    f_wmp_init          wmp_init;       ///< Initialization routine
    f_wmp_setattr       wmp_setattr;    ///< Attribute set routine
    f_wmp_configure     wmp_configure;  ///< Configure routine
    f_wmp_embed         wmp_embed;      ///< Embed routine
    f_wmp_uninit        wmp_uninit;     ///< Uninit
    f_wmp_getattr       wmp_getattr;    ///< Get attribute routine
    f_wmp_version       wmp_version;    ///< Get version of OTT WM plugin
    f_wmp_itos          wmp_itos;       ///< Helper function int to string
    f_wmp_ftos          wmp_ftos;       ///< Helper function float to string

    /**
    ****************************************************************************
    * @brief    WM plugin attribute list
    * @note     In FFmpeg video filter pipeline, these are passed as command
    *           line arguments to run the video filter. Pipeline internally
    *           handles argument parsing and make sure that the ones passed
    *           will end up in video filter context structure. In real
    *           integration case, these attributes have to be set properly
    ****************************************************************************
    */
    const char* firstbit;
    const char* lastbit;
    const char* firstframe;
    const char* lastframe;
    const char* oid;
    const char* tmid;
    const char* profile;
    const char* banner;
    const char* logfile;

    ///< Customized attributes to run special plugins
    const char* wmtime;

    /**
    ****************************************************************************
    * @brief    Indication of the WM plugin state in current moment of time
    ****************************************************************************
    */
    VF_STATE state;

} IrdetoContext;

#endif /* !_VF_OTT_CTX_H_ */

/**
********************************************************************************
* @file         ir_xps_export_test.c
* @brief        Irdeto unit-test implementation for xps data export interface
* @author       Yu Duan
* @date         May 10, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/
#include <stdlib.h>
#include <string.h>
#include <memory>

extern "C"
{
    #include "irxps/ir_xps_common.h"
    #include "irxps/ir_xps_import_hevc.h"
    #include "test_types.h"
}

/* Mocked destroy method */
void ir_xps_context_destroy(ir_xps_context* xps_context)
{
    if (xps_context)
    {
        free(xps_context);
    }
}

auto api_deleter = [](const x265_api* api){
    if (api)
    {
        api->cleanup();
    }
};
std::shared_ptr<const x265_api> api = { x265_api_get(8), api_deleter };

auto param_deleter = [](x265_param* p)
{
    if (p)
    {
        api->param_free(p);
    }
};

std::unique_ptr<x265_param, decltype(param_deleter)> param(api->param_alloc(),
    param_deleter);

auto xps_deleter = [](ir_xps_context* xps)
{
    ir_xps_context_destroy(xps);
};

std::unique_ptr<ir_xps_context, decltype(xps_deleter)> xps(
    ir_xps_context_create(), xps_deleter);

TR_TYPE ir_test_xps_common_flow()
{
    IR_XPS_EXPORT_STATUS result = ir_xps_vc(xps.get());
    return (IR_XPS_EXPORT_STATUS_OK == result) ? TR_TYPE_PASS : TR_TYPE_FAIL;
}

TR_TYPE ir_test_xps_corrupted_ctx()
{
    ir_xps_context* xps = ir_xps_context_create();
    xps++;
    IR_XPS_EXPORT_STATUS result = ir_xps_vc(xps);
    xps--;
    ir_xps_context_destroy(xps);
    return (IR_XPS_EXPORT_STATUS_FAIL == result) ? TR_TYPE_PASS : TR_TYPE_FAIL;
}

TR_TYPE ir_test_xps_import_x265_null()
{
    IR_XPS_EXPORT_STATUS result = ir_xps_import_meta_x265(NULL, xps.get());
    return (IR_XPS_EXPORT_STATUS_BADARG == result) ? TR_TYPE_PASS : TR_TYPE_FAIL;
}

TR_TYPE ir_test_xps_import_x265_unready(void)
{
    IR_XPS_EXPORT_STATUS result = ir_xps_import_meta_x265(param.get(),
        xps.get());
    return (IR_XPS_EXPORT_STATUS_BADARG == result) ? TR_TYPE_PASS : TR_TYPE_FAIL;
}

TR_TYPE ir_test_xps_import_x265_ready(void)
{
    xps->header.state = IR_CONTEXT_STATE_READY;
    IR_XPS_EXPORT_STATUS result = ir_xps_import_meta_x265(param.get(),
        xps.get());
    return (IR_XPS_EXPORT_STATUS_OK == result) ? TR_TYPE_PASS : TR_TYPE_FAIL;
}

TR_TYPE ir_test_xps_import_x265_slices(void)
{
    xps->header.state = IR_CONTEXT_STATE_READY;
    xps->hevc_meta.sps.pic_height_in_luma_samples = 1280;
    IR_XPS_EXPORT_STATUS result = ir_xps_import_meta_x265(param.get(),
        xps.get());
    TR_TYPE tr = (IR_XPS_EXPORT_STATUS_OK == result) ? TR_TYPE_PASS :
        TR_TYPE_FAIL;
    if (param->maxSlices <= 0 || param->maxSlices > 8)
    {
        tr = TR_TYPE_FAIL;
    }
    return tr;
}

TR_TYPE ir_test_xps_import_x265_tiles(void)
{
    xps->header.state = IR_CONTEXT_STATE_READY;
    xps->hevc_meta.pps.tiles_enabled_flag = 1;
    xps->hevc_meta.sps.pic_width_in_luma_samples = 1920;
    xps->hevc_meta.sps.pic_height_in_luma_samples = 1080;
    IR_XPS_EXPORT_STATUS result = ir_xps_import_meta_x265(param.get(),
        xps.get());
    TR_TYPE tr = (IR_XPS_EXPORT_STATUS_OK == result) ? TR_TYPE_PASS :
        TR_TYPE_FAIL;
    if (param->maxSlices != 1 || param->tileRows <= 0 ||
        param->tileColumns <= 0)
    {
        tr = TR_TYPE_FAIL;
    }
    return tr;
}

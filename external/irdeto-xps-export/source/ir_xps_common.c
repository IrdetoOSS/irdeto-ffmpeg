/**
********************************************************************************
* @file         ir_xps_common.h
* @brief        Common implementation shared by export and import interface
* @author       Yu Duan (yu.duan@irdeto.com)
* @date         Mar 09, 2017
* @copyright    Irdeto B.V. All rights reserved.
********************************************************************************
*/
#include "irxps/ir_xps_common.h"

#include <stdlib.h>
#include <string.h>

ir_xps_context* ir_xps_context_create()
{
    ir_xps_context* xps_context = NULL;
    do
    {
        xps_context = (ir_xps_context*) malloc(sizeof(ir_xps_context));
        if (NULL == xps_context)
        {
            break;
        }

        memset(xps_context, 0, sizeof(ir_xps_context));
        xps_context->header.size = sizeof(ir_xps_context);
        xps_context->header.state = IR_CONTEXT_STATE_INITIALIZED;
    } while(0);

    return xps_context;
}

IR_XPS_EXPORT_STATUS ir_xps_vc(const ir_xps_context* const xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_FAIL;

    do
    {
        if (NULL == xps_context)
        {
            break;
        }

        if (sizeof(ir_xps_context) != xps_context->header.size)
        {
            break;
        }

        if (IR_CONTEXT_STATE_UNINITIALIZED == xps_context->header.state)
        {
            break;
        }

        result = IR_XPS_EXPORT_STATUS_OK;

    } while(0);

    return result;
}

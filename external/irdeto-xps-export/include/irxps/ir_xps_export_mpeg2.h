/*
 * ir_xps_export_mpeg2.h
 *
 *  Created on: Sep 28, 2018
 *      Author: michael
 */

#ifndef INCLUDE_IRXPS_IR_XPS_EXPORT_MPEG2_H_
#define INCLUDE_IRXPS_IR_XPS_EXPORT_MPEG2_H_

#include "ir_xps_common.h"

/**
********************************************************************************
* @note         Forward declaration of MPEG2 related structure and function
********************************************************************************
*/
struct MpegEncContext;
struct Picture;

IR_XPS_EXPORT_STATUS ir_xps_export_mpg2(const struct MpegEncContext* const h,
    ir_xps_context* xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == h)
        {
            break;
        }

        if (NULL == xps_context)
        {
            break;
        }

        ir_xps_export_mpeg2* xps = &(xps_context->mpeg2_meta);

        xps->width = h->width;
   		xps->height = h->height;
   		xps->progressive_sequence = h->progressive_sequence;
   		xps->chroma_format = h->chroma_format;
   		xps->picture_coding_type = h->pict_type;

   		for (int i = 0; i < 64; i++)
        {
            int j = h->idsp.idct_permutation[i];
        	xps->intra_quantiser_matrix[i] = h->intra_matrix[j];
        	xps->non_intra_quantiser_matrix[i] = h->inter_matrix[j];
        	xps->chroma_intra_quantiser_matrix[i] = h->chroma_intra_matrix[j];
        	xps->chroma_non_intra_quantiser_matrix[i] = h->chroma_inter_matrix[j];
        }

   		xps->f_code00 = h->mpeg_f_code[0][0];
   		xps->f_code01 = h->mpeg_f_code[0][1];
   		xps->f_code10 = h->mpeg_f_code[1][0];
   		xps->f_code11 = h->mpeg_f_code[1][1];
  		xps->intra_dc_precision = h->intra_dc_precision;
   		xps->picture_structure = h->picture_structure;
   		xps->top_field_first = h->top_field_first;
   		xps->frame_pred_frame_dct = h->frame_pred_frame_dct;
   		xps->concealment_motion_vectors = h->concealment_motion_vectors;
   		xps->q_scale_type = h->q_scale_type;
   		xps->intra_vlc_format = h->intra_vlc_format;
   		xps->alternate_scan = h->alternate_scan;
   		xps->repeat_first_field = h->repeat_first_field;
   		xps->chroma_420_type = h->chroma_420_type;
   		xps->progressive_frame= h->progressive_frame;

   		xps->mb_height = h->mb_height;
   		xps->mb_width = h->mb_width;
   		xps->mb_stride = h->mb_stride;
   		xps->qscale_values = av_malloc(h->mb_height * h->mb_stride);
		memcpy(xps->qscale_values, h->qscale_values, h->mb_height * h->mb_stride);
		memset(h->qscale_values, 0, h->mb_height * h->mb_stride);

		for (int y = 0; y < h->mb_height; y++)
		{
			int q;

			for (int x = 0; x < h->mb_width; x++)
			{
				if (xps->qscale_values[y * h->mb_stride + x] != 0)
				{
					q = xps->qscale_values[y * h->mb_stride + x];
					break;
				}
			}
			for (int x = 0; x < h->mb_width; x++)
			{
				if (xps->qscale_values[y * h->mb_stride + x] != 0)
				{
					break;
				}
				xps->qscale_values[y * h->mb_stride + x] = q;
			}
			for (int x = 1; x < h->mb_width; x++)
			{
				if (xps->qscale_values[y * h->mb_stride + x] == 0)
				{
					xps->qscale_values[y * h->mb_stride + x] = xps->qscale_values[y * h->mb_stride + x - 1];
				}
			}
		}

		if (xps->picture_structure != PICT_FRAME)
		{	// combine 2 fields
			for (int y = 1; y < h->mb_height; y+=2)
			{
				for (int x = 0; x < h->mb_width; x++)
				{
					int avg = (xps->qscale_values[(y - 1) * h->mb_stride + x] + xps->qscale_values[y * h->mb_stride + x]) / 2;
					xps->qscale_values[(y - 1) * h->mb_stride + x] = avg;
					xps->qscale_values[y * h->mb_stride + x] = avg;
				}
			}
		}

         /**
         ************************************************************************
         * @note     Export references if any
         ************************************************************************
         */
        for (int k = 0; k < 2; k ++)
        {
        	Picture* src = (k == 0) ? h->last_picture_ptr : h->next_picture_ptr;
        	memset(&xps_context->ref_frame[k], 0, sizeof(ir_ref));
        	xps_context->ref_frame[k].avframe = (src == NULL) ? NULL : av_frame_clone(src->f);
        }

        /**
          ************************************************************************
          * @note 	Export decoded packet
          ************************************************************************
        */
        xps->pkt = av_packet_clone(h->pkt_strm);

        xps->guid = h->current_picture_ptr->f->pkt_pts;

	xps->is_imx = h->pict_type == AV_PICTURE_TYPE_I &&
                (h->slice_count == h->mb_width * h->mb_height) && (h->mb_width * h->mb_height != 0);

        xps_context->header.state = IR_CONTEXT_STATE_READY;
        result = IR_XPS_EXPORT_STATUS_OK;

    } while(0);

    return result;
}

#endif /* INCLUDE_IRXPS_IR_XPS_EXPORT_MPEG2_H_ */

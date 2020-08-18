/*
 * ir_xps_import_mpeg2.h
 *
 *  Created on: Sep 28, 2018
 *      Author: michael
 */

#ifndef INCLUDE_IRXPS_IR_XPS_IMPORT_MPEG2_H_
#define INCLUDE_IRXPS_IR_XPS_IMPORT_MPEG2_H_

static int find_element(uint8_t **ptr, uint8_t *end_ptr, int *len,
		int code_min, int code_max, int code_ext)
{
	int result = -1;
	uint8_t *p = *ptr;
	int i, start = -1;
	int code = -1, ext = -1;
	for (i = 0; i < end_ptr - p - 1; i++)
	{
		if ((p[i + 0] == 0x0 && p[i + 1] == 0x0 && p[i + 2] == 0x1) || (i == end_ptr - p - 2))
		{
			if (start >= 0)
			{
				int size = i - start;
				if (i == end_ptr - p - 2)
				{
					size += 3;
				}

				if (code >= code_min &&	code <= code_max &&	ext == code_ext)
				{
					(*ptr) = p + start;
					*len = size;
					result = 0;
					break;
				}
			}
			code = ext = -1;
			if (i < end_ptr - p - 2)
			{
				code = p[i + 3];
				if (i < end_ptr - p - 3 && code == 0xB5)
				{
					ext = p[i + 4] >> 4;
				}
			}
			start = i;
		}
	}
	return result;
}


IR_XPS_EXPORT_STATUS ir_xps_import_meta_mpg2(AVCodecContext * const avctx,
    const ir_xps_context* const xps_context)
{
    IR_XPS_EXPORT_STATUS result = IR_XPS_EXPORT_STATUS_BADARG;

    do
    {
        if (NULL == avctx)
        {
            break;
        }

        MpegEncContext *s = avctx->priv_data;

        if (NULL == s)
        {
        	break;
        }

        if (NULL == xps_context)
        {
            break;
        }

        if (xps_context->header.state != IR_CONTEXT_STATE_READY)
        {
            break;
        }

        const ir_xps_export_mpeg2* xps = &(xps_context->mpeg2_meta);

		/* seq ext */
		s->progressive_sequence = xps->progressive_sequence;
		s->progressive_frame = xps->progressive_frame;
		s->chroma_format = xps->chroma_format;

		/* pic coding ext */
		s->top_field_first = xps->top_field_first;
		s->current_picture_ptr->f->top_field_first = xps->top_field_first;
		s->repeat_first_field = xps->repeat_first_field;
		s->chroma_420_type = xps->chroma_420_type;

		s->q_scale_type = xps->q_scale_type;

		/* Q matrices */
		ff_convert_matrix(s, s->q_intra_matrix, s->q_intra_matrix16,
				          xps->intra_quantiser_matrix, s->intra_quant_bias, avctx->qmin,
	                      31, 1);
	    ff_convert_matrix(s, s->q_inter_matrix, s->q_inter_matrix16,
	    				  xps->non_intra_quantiser_matrix, s->inter_quant_bias, avctx->qmin,
	                      31, 0);
	    ff_convert_matrix(s, s->q_chroma_intra_matrix, s->q_chroma_intra_matrix16,
	    		          xps->chroma_intra_quantiser_matrix, s->intra_quant_bias, avctx->qmin,
	                      31, 1);
	    ff_convert_matrix(s, s->q_chroma_inter_matrix, s->q_chroma_inter_matrix16,
	    		          xps->chroma_non_intra_quantiser_matrix, s->inter_quant_bias, avctx->qmin,
   	                      31, 0);

	    /* qscale values */
		memcpy(s->qscale_values, xps->qscale_values, xps->mb_stride * xps->mb_height);
		double sum = 0.0;
		for (int y = 0; y < xps->mb_height; y++) {
			for (int x = 0; x < xps->mb_width; x++) {
				sum += s->qscale_values[y * xps->mb_stride + x];
			}
		}
		int avg_qp = ceil(sum / (xps->mb_height * xps->mb_width));
		int delta_qp = avctx->global_quality - avg_qp;
		for (int y = 0; y < xps->mb_height; y++)
		{
			for (int x = 0; x < xps->mb_width; x++) {
				int qscale_new = s->qscale_values[y * xps->mb_stride + x] + delta_qp;
				if (qscale_new < 2) {
					qscale_new = 2; /* bug ffmpeg, qscale = 1 leads to corruption */
				}
				else if (qscale_new > 31) {
					qscale_new = 31;
				}
				s->qscale_values[y * xps->mb_stride + x] = qscale_new;
			}
		}

		/* force frame type */
		s->pict_type = s->current_picture_ptr->f->pict_type;
		s->new_picture.f->pict_type = s->pict_type;

		/* imx */
		s->irdeto_imx = xps->is_imx;

	} while(0);

    return result;
}


IR_XPS_EXPORT_STATUS ir_xps_import_merge_mpg2(const struct MpegEncContext* const h,
		ir_xps_context* xps_context, AVPacket *pkt)
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

        AVPacket *pkt_src = (AVPacket *)xps_context->mpeg2_meta.pkt;

        uint8_t *src_ptr     = pkt_src->data;
   		uint8_t *src_cur_ptr = pkt_src->data;
   		uint8_t *src_end_ptr = src_ptr + pkt_src->size - 1;
        uint8_t *enc_ptr     = pkt->data;
        uint8_t *enc_cur_ptr = pkt->data;
        uint8_t *enc_end_ptr = enc_ptr + pkt->size - 1;

        uint8_t *dst_buf = malloc(pkt_src->size + pkt->size);
        uint8_t *dst_ptr = dst_buf;
        int len;

        do
        {
        	/* copy up to coding extension from source */
			if (find_element(&src_cur_ptr, src_end_ptr, &len, 0xB5, 0xB5, 0x8) != 0)
			{
				break;
			}
			memcpy(dst_ptr, src_ptr, src_cur_ptr - src_ptr);
			dst_ptr += src_cur_ptr - src_ptr;
			src_cur_ptr += len;

			/* copy coding extension from re encoded */
			if (find_element(&enc_cur_ptr, enc_end_ptr, &len, 0xB5, 0xB5, 0x8) != 0)
			{
				break;
			}
			memcpy(dst_ptr, enc_cur_ptr, len);
			dst_ptr += len;

			/* copy user data and extensions from source */
			uint8_t *tmp_ptr = src_cur_ptr;
			if (find_element(&src_cur_ptr, src_end_ptr, &len, 0x01, 0xAF, -1) != 0)
			{
				break;
			}
			memcpy(dst_ptr, tmp_ptr, src_cur_ptr - tmp_ptr);
			dst_ptr += src_cur_ptr - tmp_ptr;

			/* copy picture data from re-encoded */
			if (find_element(&enc_cur_ptr, enc_end_ptr, &len, 0x01, 0xAF, -1) != 0)
			{
				break;
			}
			memcpy(dst_ptr, enc_cur_ptr, enc_end_ptr - enc_cur_ptr + 1);
			dst_ptr += enc_end_ptr - enc_cur_ptr + 1;

			/* copy eos if any from source */
			if (find_element(&src_cur_ptr, src_end_ptr, &len, 0xB7, 0xB7, -1) == 0)
			{
				memcpy(dst_ptr, src_cur_ptr, len);
				dst_ptr += len;
			}

			/* copy to pkt */
			int new_size = dst_ptr - dst_buf;
	        if (new_size > pkt->size)
	        {
	      		av_grow_packet(pkt, new_size - pkt->size);
	        }
	        else
	        {
	        	av_shrink_packet(pkt, new_size);
	        }
	        memcpy(pkt->data, dst_buf, new_size);

			result = IR_XPS_EXPORT_STATUS_OK;
        }
		while(0);

        free(dst_buf);

    } while(0);

    return result;
}


#endif /* INCLUDE_IRXPS_IR_XPS_IMPORT_MPEG2_H_ */

#include <string>

#include "ffmpeg_encoder.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

int encode(AVFrame* const frame, AVPacket* const packet, int annexb, int non_vcl, int codec_id, int qp)
{
    int result = 0;
    AVCodec* codec = NULL;
    AVCodecContext* encode_ctx = NULL;
    AVDictionary* opts = NULL;

    do
    {
        codec = avcodec_find_encoder(static_cast<AVCodecID>(codec_id));
        if (NULL == codec)
        {
            result = -1;
            break;
        }

        encode_ctx = avcodec_alloc_context3(codec);
        if (NULL == encode_ctx)
        {
            result = -1;
            break;
        }

        encode_ctx->pix_fmt = static_cast<AVPixelFormat>(frame->format);
        encode_ctx->width = frame->width;
        encode_ctx->height = frame->height;
        encode_ctx->thread_count = 1;
        encode_ctx->time_base = (AVRational){1, 25};
        encode_ctx->framerate = (AVRational){25, 1};
        encode_ctx->global_quality = qp;
        av_dict_set(&opts, "irdeto_exports", "1", 0);

        /**
        * @TODO:    Decide whether use returned PPS based on annexb flag.
        *           It's illegal to remux PPS into mdat in case of MP4 format, in such case we should find a way
        *           to put custom PPS into moov->trak->mdia->minf->stbl->stsd->avc1
        */
        av_dict_set(&opts, "irdeto_pps_id", "11", 0);
        av_dict_set(&opts, "irdeto_non_vcl", non_vcl ? "1" : "0", 0);

        ///< Log level is set to warning
        if (codec_id == AV_CODEC_ID_H264)
        {
            av_dict_set(&opts, "x264-params", annexb ? "annexb=1:log=3:cabac=0" : "annexb=0:log=3:cabac=0", 0);
        }
        else
        {
            av_dict_set(&opts, "x265-params", annexb ? "annexb=1:log=1" : "annexb=0:log=1", 0);
        }

        result = avcodec_open2(encode_ctx, codec, &opts);
        if (result < 0)
        {
            break;
        }

        result = avcodec_send_frame(encode_ctx, frame);
        if (result < 0)
        {
            fflush(stdout);
            char buf[128];
            fprintf(stderr, "avcodec_send_frame returns error: %s\n", av_make_error_string(buf, 128, result));
            break;
        }

        result = avcodec_receive_packet(encode_ctx, packet);
        if (result < 0)
        {
            break;
        }

    } while(0);

    av_dict_free(&opts);
    avcodec_free_context(&encode_ctx);
    return result;
}


#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "irxps/ir_xps_common.h"
#include "irxps/ir_xps_export_ref.h"
}

#include "ffmpeg_encoder.h"

struct pts_matcher_t
{
    explicit pts_matcher_t(int64_t pts) : _pts(pts) {}
    bool operator()(const AVPacket& pkt) const { return pkt.pts == _pts; }
    int64_t _pts;
};

static inline char* get_error_string(int code)
{
    static char errbuf[128];
    return av_make_error_string(errbuf, sizeof(errbuf), code);
}

int decode_frame(AVCodecContext* decode_ctx, AVFrame* const frame, AVPacket* const packet, int flush)
{
    int result = 0;

    do
    {
        if (flush)
        {
            packet->size = 0;
            packet->data = NULL;
            packet->pts = 0;
        }

        decode_ctx->opaque = NULL;
        result = avcodec_send_packet(decode_ctx, packet);
        if (0 != result && !(result == AVERROR_EOF && packet->size == 0))
        {
            break;
        }

        result = avcodec_receive_frame(decode_ctx, frame);
    } while(0);

    return result;
}

static void usage()
{
    std::cerr << "irdeto-codec-util <fin> <fout> <qp> [i]" << std::endl;
    std::cerr << "    fin : input_elementary_stream" << std::endl;
    std::cerr << "    fout: output_elementary_stream" << std::endl;
    std::cerr << "    i   : re-encode intra frames"  << std::endl;
}

int main(int argc, char const *argv[])
{
    int result = -1;
    AVFormatContext* demux_ctx = NULL;
    AVFormatContext* remux_ctx = NULL;
    AVCodecContext* decode_ctx = NULL;
    AVStream* video_stream = NULL;
    AVFrame* frame = NULL;
    AVPacket packet;
    std::list<AVPacket> dts_order_packets;
    int64_t packet_guid = 0, display_guid = 0;
    int video_stream_index = -1;
    int qp = 0;
    bool intra_only = false;

    do
    {
        if (argc !=4 && argc != 5)
        {
            usage();
            break;
        }
        qp = atoi(argv[3]);
        if (qp <= 0 || qp > 51)
        {
            std::cerr << "<qp> value must be in range [1, 51]" << std::endl;
            break;
        }

        if (argc == 5)
        {
            if (strcmp(argv[4], "i") != 0)
            {
                std::cerr << "Invalid argument, " << argv[4] << std::endl;
                usage();
                break;
            }
            intra_only = true;
        }

        av_log_set_level(AV_LOG_WARNING);

        /* Prepare demuxer */
        result = avformat_open_input(&demux_ctx, argv[1], NULL, NULL);
        if (result < 0)
        {
            std::cerr << "avformat_open_input failed on input: " << argv[1] << "Error: " << get_error_string(result) << std::endl;
            break;
        }
        demux_ctx->flags |= AVFMT_FLAG_GENPTS;

        result = avformat_find_stream_info(demux_ctx, NULL);
        if (result < 0)
        {
            std::cerr << "avformat_find_stream_info failed. Error: " << get_error_string(result) << std::endl;
            break;
        }

        video_stream_index = av_find_best_stream(demux_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (video_stream_index < 0)
        {
            std::cerr << "Can't find proper video stream from input " << argv[1] << std::endl;
            break;
        }

        /* Prepare remuxer */
        result = avformat_alloc_output_context2(&remux_ctx, NULL, NULL, argv[2]);
        if (result < 0)
        {
            std::cerr << "avformat_alloc_output_context2 failed on input " << argv[2] << ". Error: " << get_error_string(result) << std::endl;
            break;
        }

        for (uint32_t i = 0; i < demux_ctx->nb_streams; i++)
        {
            const AVStream* in = demux_ctx->streams[i];
            AVStream* out = avformat_new_stream(remux_ctx, NULL);
            avcodec_parameters_copy(out->codecpar, in->codecpar);
        }
        remux_ctx->oformat->flags |= AVFMT_NOTIMESTAMPS;
        remux_ctx->oformat->flags |= AVFMT_TS_NONSTRICT;

        if (!(remux_ctx->oformat->flags & AVFMT_NOFILE))
        {
            result = avio_open(&remux_ctx->pb, argv[2], AVIO_FLAG_WRITE);
            if (result < 0)
            {
                std::cerr << "avio_open failed on output " << argv[2] << ". Error: " << get_error_string(result) << std::endl;
                break;
            }
        }

        /* Check if codec_id is H264 or H265 */
        video_stream = demux_ctx->streams[video_stream_index];
        int codec_id = video_stream->codecpar->codec_id;
        if (codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_H265)
        {
            std::cerr << "Only H264 and H265 are supported at this moment." << std::endl;
            break;
        }

        /* Prepare decoder */
        AVCodec* dec = avcodec_find_decoder(static_cast<AVCodecID>(codec_id));
        decode_ctx = avcodec_alloc_context3(dec);
        if (NULL == decode_ctx)
        {
            std::cerr << "Failed to allocate AVCodecContext for decoder." << std::endl;
            break;
        }

        decode_ctx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
        decode_ctx->flags2 |= AV_CODEC_FLAG2_IGNORE_CROP | AV_CODEC_FLAG2_SHOW_ALL;
        decode_ctx->err_recognition = AV_EF_CRCCHECK  | AV_EF_BITSTREAM | AV_EF_BUFFER;
        decode_ctx->thread_type = FF_THREAD_FRAME;
        decode_ctx->thread_count = 4;
        decode_ctx->has_b_frames = 16; ///< This will postpone decoder's output but it prevents leaks
        avcodec_parameters_to_context(decode_ctx, video_stream->codecpar);
        AVDictionary* opts = NULL;
        av_dict_set(&opts, "irdeto_exports", "1", 0);
        result = avcodec_open2(decode_ctx, dec, &opts);
        if (result < 0)
        {
            av_dict_free(&opts);
            std::cerr << "avcodec_open2 failed. Error: " << get_error_string(result) << std::endl;
            break;
        }
        av_dict_free(&opts);

        frame = av_frame_alloc();
        av_init_packet(&packet);

        result = avformat_write_header(remux_ctx, NULL);
        if (result < 0)
        {
            std::cerr << "avformat_write_header failed. Error: " << get_error_string(result) << std::endl;
            break;
        }

        while (true)
        {
            result = av_read_frame(demux_ctx, &packet);
            if (result != 0)
            {
                break;
            }

            packet.pts = packet_guid++;

            /* Implicit copy */
            dts_order_packets.push_back(packet);
            if (packet.stream_index != video_stream_index)
            {
                continue;
            }

            if (0 == decode_frame(decode_ctx, frame, &packet, 0))
            {
                ir_xps_context* xps = static_cast<ir_xps_context*>(frame->opaque);
                if (xps && ((frame->pict_type == AV_PICTURE_TYPE_B && !intra_only && !xps->is_ref) ||
                            (frame->pict_type == AV_PICTURE_TYPE_I && intra_only)))
                {
                    std::list<AVPacket>::iterator it = std::find_if(dts_order_packets.begin(), dts_order_packets.end(),
                        pts_matcher_t(frame->pts));
                    int64_t pts = (*it).pts;
                    int64_t dts = (*it).dts;

                    /*
                    * @note     If original encoded picture is too small, then skip re-encoding. Because there is no
                    *           much space to preserve information anyway, encoder will produce identical frame pixels.
                    */
                    int orig_size = (*it).size;
                    result = encode(frame, &(*it), 1, 1, codec_id, qp);
                    if (0 != result)
                    {
                        ir_xps_context_destroy(xps);
                        av_frame_unref(frame);
                        break;
                    }
                    std::cerr << "Frame " << frame->pts << "/" << display_guid << "/. Size: " << orig_size << "/" << (*it).size << std::endl;
                    (*it).pts = pts;
                    (*it).dts = dts;
                    (*it).stream_index = video_stream_index;
                }
                ir_xps_context_destroy(xps);
                av_frame_unref(frame);
                display_guid++;
            }
        }

        /* Flush frames out of decoder and release memory */
        while (decode_frame(decode_ctx, frame, &packet, 1) != AVERROR_EOF)
        {
            av_packet_unref(&packet);
            ir_xps_context_destroy(static_cast<ir_xps_context*>(frame->opaque));
            av_frame_unref(frame);
        }
        av_frame_free(&frame);

        if (result == AVERROR_EOF || result == 0)
        {
            while (!dts_order_packets.empty())
            {
                AVPacket& packet = dts_order_packets.front();
                const AVStream* in_stream = demux_ctx->streams[packet.stream_index];
                const AVStream* out_stream = remux_ctx->streams[packet.stream_index];
                packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base,
                    static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base,
                    static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
                result = av_interleaved_write_frame(remux_ctx, &packet);
                if (0 != result)
                {
                    std::cerr << "av_write_frame failed. Error: " << get_error_string(result) << std::endl;
                    break;
                }

                av_packet_unref(&packet);
                dts_order_packets.pop_front();
            }

            av_write_trailer(remux_ctx);
        }

        if (result == AVERROR_EOF)
        {
            result = 0;
        }

    } while(0);

    if (remux_ctx && remux_ctx->pb)
    {
        avio_close(remux_ctx->pb);
    }

    avformat_close_input(&demux_ctx);
    avformat_free_context(demux_ctx);
    avformat_free_context(remux_ctx);
    avcodec_free_context(&decode_ctx);

    return result;
}

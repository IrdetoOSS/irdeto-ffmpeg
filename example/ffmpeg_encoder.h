
#ifndef _IR_FFMPEG_ENCODER_H_
#define _IR_FFMPEG_ENCODER_H_

struct AVFrame;
struct AVPacket;

int encode(AVFrame* const frame, AVPacket* const packet, int annexb, int non_vcl, int codec_id, int qp);

#endif  /* !_IR_FFMPEG_ENCODER_H_ */

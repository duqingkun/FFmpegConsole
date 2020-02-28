#include "decoder.h"
#include <iostream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

void Decoder::decode(const char *filename, const char *out_video, const char *out_audio)
{
	Context context;
	memset(&context, 0, sizeof(Context));
	if(!open_input(&context, filename))
	{
		return;
	}
	av_dump_format(context.fmt_ctx, 0, filename, 0);

	FILE *fVideo = nullptr;
	FILE *fAudio = nullptr;
	if(context.hasVideo && out_video != nullptr)
	{
		fopen_s(&fVideo, out_video, "wb+");
		context.sws_ctx = sws_getContext(context.v_codec_ctx->width, context.v_codec_ctx->height,
			context.v_codec_ctx->pix_fmt, context.v_codec_ctx->width, context.v_codec_ctx->height,
			AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);
		context.v_frm = av_frame_alloc();
	}

	if (context.hasAudio && out_audio != nullptr)
	{
		fopen_s(&fAudio, out_audio, "wb+");
	}

	AVPacket pkt;
	av_init_packet(&pkt);

	while (av_read_frame(context.fmt_ctx, &pkt) >= 0)
	{
		decode_packet(&context, &pkt, fAudio, fVideo);
	}

	//flush decoder
	pkt.data = nullptr;
	pkt.size = 0;
	decode_packet(&context, &pkt, fAudio, fVideo);

	if(context.v_frm != nullptr) av_frame_free(&context.v_frm);
	close(&context);
	if (fAudio != nullptr) fclose(fAudio);
	if (fVideo != nullptr) fclose(fVideo);
}

bool Decoder::open_input(Context *ctx, const char *filename)
{
	ctx->hasAudio = false;
	ctx->hasVideo = false;
	int ret = avformat_open_input(&ctx->fmt_ctx, filename, nullptr, nullptr);
	if (ret < 0)
	{
		return false;
	}

	ret = avformat_find_stream_info(ctx->fmt_ctx, nullptr);
	if (ret < 0)
	{
		return false;
	}

	ctx->a_index = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (ctx->a_index >= 0)
	{
		ctx->a_codec = avcodec_find_decoder(ctx->fmt_ctx->streams[ctx->a_index]->codecpar->codec_id);
		if (ctx->a_codec != nullptr)
		{
			ctx->a_codec_ctx = avcodec_alloc_context3(ctx->a_codec);
			if (ctx->a_codec_ctx != nullptr)
			{
				ret = avcodec_parameters_to_context(ctx->a_codec_ctx, ctx->fmt_ctx->streams[ctx->a_index]->codecpar);
				if (ret >= 0)
				{
					ret = avcodec_open2(ctx->a_codec_ctx, ctx->a_codec, nullptr);
					ctx->hasAudio = (ret >= 0);
				}
			}
		}
	}

	ctx->v_index = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (ctx->v_index >= 0)
	{
		ctx->v_codec = avcodec_find_decoder(ctx->fmt_ctx->streams[ctx->v_index]->codecpar->codec_id);
		if (ctx->v_codec != nullptr)
		{
			ctx->v_codec_ctx = avcodec_alloc_context3(ctx->v_codec);
			if (ctx->v_codec_ctx != nullptr)
			{
				ret = avcodec_parameters_to_context(ctx->v_codec_ctx, ctx->fmt_ctx->streams[ctx->v_index]->codecpar);
				if (ret >= 0)
				{
					ret = avcodec_open2(ctx->v_codec_ctx, ctx->v_codec, nullptr);
					ctx->hasVideo = (ret >= 0);
				}
			}
		}
	}

	return true;
}

void Decoder::close(Context *ctx)
{
	if (ctx->a_codec_ctx != nullptr)
	{
		avcodec_free_context(&ctx->a_codec_ctx);
	}
	if (ctx->v_codec_ctx != nullptr)
	{
		avcodec_free_context(&ctx->v_codec_ctx);
	}
	avformat_close_input(&ctx->fmt_ctx);
}

void Decoder::decode_packet(Context *ctx, AVPacket *pkt, FILE *fAudio, FILE *fVideo)
{
	AVFrame *frm = av_frame_alloc();
	int ret = -1;
	if(pkt->stream_index == ctx->a_index) //音频
	{
		ret = avcodec_send_packet(ctx->a_codec_ctx, pkt);
		if(ret < 0)
		{
			return;
		}
		while (ret >= 0)
		{
			ret = avcodec_receive_frame(ctx->a_codec_ctx, frm);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
			{
				break;
			}

			double timestamp = frm->pts * av_q2d(ctx->a_codec_ctx->time_base);
			//std::cout << CYAN << "Audio timestamp: " << timestamp << "s" << std::endl;
			int data_size = av_get_bytes_per_sample(ctx->a_codec_ctx->sample_fmt);
			if(data_size < 0)
			{
				break;
			}
			if(fAudio != nullptr)
			{
				for (int i = 0; i < frm->nb_samples; i++)
					for (int ch = 0; ch < ctx->a_codec_ctx->channels; ch++)
						fwrite(frm->data[ch] + data_size * i, 1, data_size, fAudio);
			}
		}
		
	}
	else if(pkt->stream_index == ctx->v_index)
	{
		ret = avcodec_send_packet(ctx->v_codec_ctx, pkt);
		if (ret < 0)
		{
			return;
		}
		
		while (ret >= 0)
		{
			ret = avcodec_receive_frame(ctx->v_codec_ctx, frm);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
			{
				break;
			}
			
			double timestamp = frm->pts * av_q2d(ctx->fmt_ctx->streams[ctx->v_index]->time_base);
			//std::cout << YELLOW << "Video timestamp: " << timestamp << "s" << std::endl;
			if(fVideo != nullptr)
			{
				if(ctx->sws_ctx != nullptr)
				{
					//为yuv420p_frm中的data,linesize指针分配空间
					if(ctx->v_frm->data[0] == nullptr)
					{
						ret = av_image_alloc(ctx->v_frm->data, ctx->v_frm->linesize,
							ctx->v_codec_ctx->width, ctx->v_codec_ctx->height, AV_PIX_FMT_YUV420P, 1);
					}

					//转换
					ret = sws_scale(ctx->sws_ctx, frm->data, frm->linesize, 0, ctx->v_codec_ctx->height,
						ctx->v_frm->data, ctx->v_frm->linesize);

					int ySize = ctx->v_codec_ctx->width * ctx->v_codec_ctx->height;
					int uSize = ySize / 4;
					int vSize = ySize / 4;
					fwrite(ctx->v_frm->data[0], 1, ySize, fVideo);
					fwrite(ctx->v_frm->data[1], 1, uSize, fVideo);
					fwrite(ctx->v_frm->data[2], 1, vSize, fVideo);
				}
				else
				{
					//不做转换，则可能会有填充得空白数据，比如688*384的视频会填充成768*384，data中每行结尾都有无效数据
					int ySize = frm->linesize[0] * ctx->v_codec_ctx->height;// ctx->v_codec_ctx->width * ctx->v_codec_ctx->height;
					int uSize = ySize / 4;
					int vSize = ySize / 4;
					fwrite(frm->data[0], 1, ySize, fVideo);
					fwrite(frm->data[1], 1, uSize, fVideo);
					fwrite(frm->data[2], 1, vSize, fVideo);
				}
			}
		}
	}
}

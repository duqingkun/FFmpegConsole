#include "decoder.h"
#include <iostream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

void Decoder::decode(const char *filename, const char *out_video, const char *out_audio, Param *param)
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
	Param p;
	memset(&p, 0, sizeof(Param));
	if(context.hasVideo && out_video != nullptr)
	{
		fopen_s(&fVideo, out_video, "wb+");
		
		p.video.width = context.v_codec_ctx->width;
		p.video.height = context.v_codec_ctx->height;
		p.video.fmt = context.v_codec_ctx->pix_fmt;
		if(param != nullptr)
		{
			if(param->video.width >= 0) p.video.width = param->video.width;
			if (param->video.height >= 0) p.video.height = param->video.height;
			if (param->video.fmt >= 0) p.video.fmt = param->video.fmt;
		}
		context.sws_ctx = sws_getContext(context.v_codec_ctx->width, context.v_codec_ctx->height,
			context.v_codec_ctx->pix_fmt, p.video.width, p.video.height, p.video.fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
		context.v_frm = av_frame_alloc();
	}

	if (context.hasAudio && out_audio != nullptr)
	{
		fopen_s(&fAudio, out_audio, "wb+");

		p.audio.ch_layout = context.a_codec_ctx->channel_layout;
		p.audio.rate = context.a_codec_ctx->sample_rate;
		p.audio.fmt = context.a_codec_ctx->sample_fmt;
		if (param != nullptr)
		{
			if(param->audio.ch_layout >= 0) p.audio.ch_layout = param->audio.ch_layout;
			if(param->audio.rate >= 0) p.audio.rate = param->audio.rate;
			if(param->audio.fmt >= 0) p.audio.fmt = param->audio.fmt;
		}

		p.audio.__nb_channels = av_get_channel_layout_nb_channels(param->audio.ch_layout);
		
		context.swr_ctx = swr_alloc();
		av_opt_set_int(context.swr_ctx, "in_channel_layout", context.a_codec_ctx->channel_layout, 0);
		av_opt_set_int(context.swr_ctx, "in_sample_rate", context.a_codec_ctx->sample_rate, 0);
		av_opt_set_sample_fmt(context.swr_ctx, "in_sample_fmt", context.a_codec_ctx->sample_fmt, 0);

		av_opt_set_int(context.swr_ctx, "out_channel_layout", p.audio.ch_layout, 0);
		av_opt_set_int(context.swr_ctx, "out_sample_rate", p.audio.rate, 0);
		av_opt_set_sample_fmt(context.swr_ctx, "out_sample_fmt", p.audio.fmt, 0);

		swr_init(context.swr_ctx);
		context.a_frm = av_frame_alloc();
	}

	AVPacket pkt;
	av_init_packet(&pkt);

	while (av_read_frame(context.fmt_ctx, &pkt) >= 0)
	{
		decode_packet(&context, &pkt, fAudio, fVideo, &p);
	}

	//flush decoder
	pkt.data = nullptr;
	pkt.size = 0;
	decode_packet(&context, &pkt, fAudio, fVideo, &p);

	if(context.v_frm != nullptr) av_frame_free(&context.v_frm);
	if (context.a_frm != nullptr) av_frame_free(&context.a_frm);
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

void Decoder::decode_packet(Context *ctx, AVPacket *pkt, FILE *fAudio, FILE *fVideo, Param *param)
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
				if(ctx->swr_ctx != nullptr)
				{
					if (ctx->a_frm->data[0] == nullptr)
					{
						int dst_nb_samples = av_rescale_rnd(swr_get_delay(ctx->swr_ctx, ctx->a_codec_ctx->sample_rate) + frm->nb_samples, 
							param->audio.rate, ctx->a_codec_ctx->sample_rate, AV_ROUND_UP);
						if(dst_nb_samples > param->audio.__nb_samples)
						{
							if(ctx->a_frm->data[0] != nullptr)
							{
								av_freep(&ctx->a_frm->data[0]);
							}
							
							ret = av_samples_alloc(ctx->a_frm->data, ctx->a_frm->linesize, param->audio.__nb_channels, dst_nb_samples, param->audio.fmt, 1);
							param->audio.__nb_samples = dst_nb_samples;
						}
					}
					ret = swr_convert(ctx->swr_ctx, ctx->a_frm->data, param->audio.__nb_samples, (const uint8_t**)frm->data, frm->nb_samples);
					if(ret < 0)
					{
						break;
					}
					int dst_bufsize = av_samples_get_buffer_size(ctx->a_frm->linesize, param->audio.__nb_channels, ret, param->audio.fmt, 1);
					if (dst_bufsize < 0) 
					{
						break;
					}
					fwrite(ctx->a_frm->data[0], 1, dst_bufsize, fAudio);
				}
				else
				{
					for (int i = 0; i < frm->nb_samples; i++)
						for (int ch = 0; ch < ctx->a_codec_ctx->channels; ch++)
							fwrite(frm->data[ch] + data_size * i, 1, data_size, fAudio);
				}
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
							param->video.width, param->video.height, param->video.fmt, 1);
					}

					//转换
					ret = sws_scale(ctx->sws_ctx, frm->data, frm->linesize, 0, ctx->v_codec_ctx->height,
						ctx->v_frm->data, ctx->v_frm->linesize);

					int ySize = param->video.width * param->video.height;
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

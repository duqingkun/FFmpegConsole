#pragma once

#include <cstdio>

extern "C"
{
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
}

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#define Default -1

class AVFormatContext;
class AVCodec;
class AVCodecContext;
class AVPacket;
class SwsContext;
class SwrContext;
class AVFrame;

typedef struct _AudioParam
{
	int rate;
	int ch_layout;
	AVSampleFormat fmt;

	//private
	int __nb_channels;
	int __nb_samples;
	int __src_nb_samples;
	
}AudioParam;

typedef struct _VideoParam
{
	int width;
	int height;
	AVPixelFormat fmt;
}VideoParam;

typedef struct _Param
{
	AudioParam audio;
	VideoParam video;
}Param;

typedef struct _Context
{
	AVFormatContext *fmt_ctx;
	AVCodec *a_codec;
	AVCodec *v_codec;
	AVCodecContext *a_codec_ctx;
	AVCodecContext *v_codec_ctx;
	SwsContext *sws_ctx;
	SwrContext *swr_ctx;
	AVFrame *v_frm;
	AVFrame *a_frm;
	int a_index;
	int v_index;
	bool hasAudio;
	bool hasVideo;
}Context;

class Decoder
{
public:
	static void decode(const char *filename, const char *out_video = nullptr, const char *out_audio = nullptr, Param *param = nullptr);
private:
	static bool open_input(Context *ctx, const char *filename);
	static void close(Context *ctx);
	static void decode_packet(Context *ctx, AVPacket *pkt, FILE *fAudio, FILE *fVideo, Param *param);
};


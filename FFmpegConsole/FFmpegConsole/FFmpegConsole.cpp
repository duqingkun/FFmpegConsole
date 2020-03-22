#include <iostream>
#include "decoder.h"

extern "C"
{
#include <libavutil/channel_layout.h>
}

int main()
{
	const char *input = "D:\\CloudMusic\\笔记 周笔畅.mp3";
	const char *output_audio = "D:\\audio_44100_S16P.pcm";
	const char *output_video = nullptr;
	
	Param p;
	p.video.width = Default;
	p.video.height = Default;
	p.video.fmt = AV_PIX_FMT_YUV420P;

	p.audio.ch_layout = AV_CH_LAYOUT_MONO;
	p.audio.rate = 44100;
	p.audio.fmt =  AV_SAMPLE_FMT_S16P;
	Decoder::decode(input,
		/*"D:\\AV\\test_data\\01.yuv"*/nullptr,
		output_audio, &p);
	
    std::cout << RESET << "Hello World!\n";
}

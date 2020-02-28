#include <iostream>
#include "decoder.h"

extern "C"
{
#include <libavutil/channel_layout.h>
}

int main()
{
	Param p;
	p.video.width = 640;
	p.video.height = 480;
	p.video.fmt = AV_PIX_FMT_YUV420P;

	p.audio.ch_layout = AV_CH_LAYOUT_MONO;
	p.audio.rate = 44100;
	p.audio.fmt = AV_SAMPLE_FMT_S16P;
	Decoder::decode("D:\\AV\\test_data\\01.flv", 
		"D:\\AV\\test_data\\01.yuv",
		"D:\\AV\\test_data\\01.pcm", &p);
	
    std::cout << RESET << "Hello World!\n";
}

#include <iostream>
#include "decoder.h"

extern "C"
{
#include <libavutil/channel_layout.h>
}

int main()
{
	Param p;
	p.video.width = Default;
	p.video.height = Default;
	p.video.fmt = AV_PIX_FMT_YUV420P;

	p.audio.ch_layout = Default;//AV_CH_LAYOUT_STEREO;
	p.audio.rate = Default;// 44100;
	p.audio.fmt = (AVSampleFormat)Default;// AV_SAMPLE_FMT_FLTP;
	Decoder::decode("D:\\AV\\test_data\\sample.mp4", 
		/*"D:\\AV\\test_data\\01.yuv"*/nullptr,
		"D:\\AV\\test_data\\sample.pcm", nullptr);
	
    std::cout << RESET << "Hello World!\n";
}

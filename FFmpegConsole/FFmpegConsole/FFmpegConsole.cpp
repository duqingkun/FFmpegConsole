#include <iostream>
#include "decoder.h"

int main()
{
	Decoder::decode("D:\\视音频编解码\\mp4_test.mp4", 
		"D:/视音频编解码/mp4_test-yuv420p.yuv", /*"D:/视音频编解码/mp4_test-yuv420p.yuv"*/
		nullptr);/*"D:/视音频编解码/mp4_test-yuv420p.pcm"*/
	
    std::cout << RESET << "Hello World!\n";
}

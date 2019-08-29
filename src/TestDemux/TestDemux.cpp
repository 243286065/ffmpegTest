// TestDemux.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

extern "C" {
#include "libavformat/avformat.h"
}

using namespace std;

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")


static double r2d(AVRational r) {
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

int main()
{
	cout << "Test Demux ffmpeg" << endl;

	const char* path = "test.mp4";

	//初始化封装库
	av_register_all();

	//初始化网络库（打开rtsp/rtmp/http协议的流媒体视频）
	avformat_network_init();

	AVDictionary *opts = NULL;
	//设置rtsp流以tcp协议打开
	av_dict_set(&opts, "rtsp_transport", "tcp", 0);

	//设置最大延时
	av_dict_set(&opts, "max_delay", "500", 0);

	//解封装上下文
	AVFormatContext *context = NULL;
	int rt = avformat_open_input(
		&context,
		path,
		0,		// 0表示自动选择解封装器
		&opts);	// 参数设置，比如rtsp得到延时时间， NULL表示默认设置

	if (rt != 0) {
		char buf[1024] = { 0 };
		av_strerror(rt, buf, sizeof(buf) - 1);
		cout << "Open " << path << " failed: " << buf << endl;
		getchar();
		return - 1;
	}

	cout << "Open " << path << " success" << endl;

	// 获取流信息
	rt = avformat_find_stream_info(context, 0);

	//总时长,毫秒
	int totalMs = context->duration / (AV_TIME_BASE / 1000);
	cout << "totalMs = " << totalMs << endl;

	//打印视频流的详细信息
	av_dump_format(context, 0, path, 0);

	// 音视频索引，区分音视频
	int vedioStreamId = 0;
	int audioStreamId = 1;
	//获取音视频流信息（遍历，函数获取）
	for (int i = 0; i < context->nb_streams; i++) {
		AVStream *as = context->streams[i];

		//音频信息
		if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStreamId = i;
			cout << i << " 音频" << endl;
			cout << "format = " << as->codecpar->format << endl;
			cout << "codec_id = " << as->codecpar->codec_id << endl;
			cout << "sample_rate = " << as->codecpar->sample_rate << endl;
			//AVSampleFormat
			cout << "channels = " << as->codecpar->channels << endl;
			//帧率fps
			cout << "audio fps = " << r2d(as->avg_frame_rate) << endl;
			// 一帧数据的单通道样本数
			cout << "frame_size = " << as->codecpar->frame_size << endl;
			// 对音频： fps = sample_rate / frame_size
		}
		//视频信息
		else if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			vedioStreamId = i;
			cout << i << " 视频" << endl;
			cout << "format = " << as->codecpar->format << endl;
			cout << "codec_id = " << as->codecpar->codec_id << endl;
			// 此处宽高并不可靠，并一定存在
			cout << "width = " << as->codecpar->width << endl;
			cout << "height = " << as->codecpar->height << endl;
			//帧率fps
			cout << "video fps = " << r2d(as->avg_frame_rate) << endl;
			cout << "frame_size = " << as->codecpar->frame_size << endl;
		}
	}

	// 获取视频流
	vedioStreamId = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, - 1, NULL, 0);
	// 获取音频流
	audioStreamId = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

	// malloc AVPaccket并初始化
	AVPacket *pkt = av_packet_alloc();
	for (;;) {
		int re = av_read_frame(context, pkt);
		if (re != 0) {
			// 读取完成，回到开头
			int ms = 3000;
			long long pos = ((double)ms / 1000) / r2d(context->streams[pkt->stream_index]->time_base);
			av_seek_frame(context, vedioStreamId, pos, AVSEEK_FLAG_BACKWARD);
			cout << "---------------------------------end--------------------" << endl;
			continue;
		}

		cout << "pkt->size = " << pkt->size << endl;

		// 显示的时间
		cout << "pkt->pts = " << pkt->pts << "	" << pkt->pts*r2d(context->streams[pkt->stream_index]->time_base) * 1000  << " ms"<<endl;
		// 解码时间
		cout << "pkt->dts = " << pkt->dts << endl;
		//pkt->pts*r2d(context->streams[pkt->stream_index]->time_base);

		if (pkt->stream_index == vedioStreamId) {
			cout << "图像" << endl;
		}

		if (pkt->stream_index == audioStreamId) {
			cout << "音频" << endl;
		}

		//释放，引用计数减1，为0时释放空间
		av_packet_unref(pkt);
	}

	av_packet_free(&pkt);

	//释放context
	if (context) {
		avformat_close_input(&context);
		context == NULL;
	}
	


	getchar();
	return 0;
}

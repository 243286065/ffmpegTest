// TestDecode.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

using namespace std;

int main()
{
	const char* path = "test.mp4";

	// 初始化封装库
	av_register_all();

	// 注册解码器
	avcodec_register_all();

	//解封装上下文
	AVFormatContext *context = NULL;
	int re = avformat_open_input(
		&context,
		path,
		0,
		NULL
	);

	if (re != 0) {
		char buf[1024] = { 0 };
		av_strerror(re, buf, 1024);
		cout << "Open " << path << " failed: " << buf << endl;
		getchar();
		return -1;
	}

	cout << "Open " << path << " success" << endl;

	//获取流信息
	re = avformat_find_stream_info(context, 0);
	//总时长,毫秒
	int totalMs = context->duration / (AV_TIME_BASE / 1000);
	cout << "totalMs = " << totalMs << endl;

	// 打印视频流的详细信息
	av_dump_format(context, 0, path, 0);

	// 音视频索引，区分音视频
	int videoStreamId = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	int audioStreamId = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0) ;

	//找到解码器
	AVCodec *vcodec = avcodec_find_decoder(context->streams[videoStreamId]->codecpar->codec_id);
	if (!vcodec) {
		cout << "Cannot find the codev id " << context->streams[videoStreamId]->codecpar->codec_id << endl;
		getchar();
		return -1;
	}
	cout << "Find the codev id " << context->streams[videoStreamId]->codecpar->codec_id << endl;

	//创建解码器上下文
	AVCodecContext *vc = avcodec_alloc_context3(vcodec);

	//配置解码器参数
	avcodec_parameters_to_context(vc, context->streams[videoStreamId]->codecpar);
	//8线程解码
	vc->thread_count = 8;

	//打开解码器上下文
	re = avcodec_open2(vc, 0, 0);
	if (re != 0) {
		char buf[1024] = { 0 };
		av_strerror(re, buf, 1024);
		cout << "Open  audio avcodec failed: " << buf << endl;
		getchar();
		return -1;
	}

	cout << "Open avcodec success "  << endl;

	//找到音频解码器
	AVCodec *acodec = avcodec_find_decoder(context->streams[audioStreamId]->codecpar->codec_id);
	if (!acodec) {
		cout << "Cannot find the codev id " << context->streams[audioStreamId]->codecpar->codec_id << endl;
		getchar();
		return -1;
	}
	cout << "Find the codev id " << context->streams[audioStreamId]->codecpar->codec_id << endl;

	//创建解码器上下文
	AVCodecContext *ac = avcodec_alloc_context3(acodec);

	//配置解码器参数
	avcodec_parameters_to_context(ac, context->streams[audioStreamId]->codecpar);
	//8线程解码
	ac->thread_count = 8;

	//打开解码器上下文
	re = avcodec_open2(ac, 0, 0);
	if (re != 0) {
		char buf[1024] = { 0 };
		av_strerror(re, buf, 1024);
		cout << "Open avcodec failed: " << buf << endl;
		getchar();
		return -1;
	}

	cout << "Open audio avcodec success " << endl;

	// 解包
	AVPacket *pkt = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	
	// 图像格式和尺寸转换的上下文
	SwsContext *vctx = NULL;
	unsigned char *rgb = NULL;

	// 音频重采样
	SwrContext *actx = swr_alloc();
	actx = swr_alloc_set_opts(
		actx,
		av_get_default_channel_layout(2),	//输出通道数
		AV_SAMPLE_FMT_S16,					//输出格式
		ac->sample_rate,					//输出采样率
		ac->channel_layout,					//输入通道数
		ac->sample_fmt,						//输入格式
		ac->sample_rate,					//输入采样率
		0,
		0
	);
	re = swr_init(actx);
	if (re != 0) {
		char buf[1024] = { 0 };
		av_strerror(re, buf, 1024);
		cout << "swr_init failed: " << buf << endl;
		getchar();
		return -1;
	}
	unsigned char *pcm = NULL;

	for (;;) {
		re = av_read_frame(context, pkt);
		if (re != 0) {
			// 读取完成
			break;
		}

		// 发送packet到解码线程
		AVCodecContext *cc = NULL;
		if (pkt->stream_index == videoStreamId) {
			cc = vc;
		}
		else if (pkt->stream_index == audioStreamId) {
			cc = ac;
		}
		// 发送packet到解码线程，不占用cpu
		re = avcodec_send_packet(cc, pkt);

		// 释放
		av_packet_unref(pkt);

		if (re != 0) {
			char buf[1024] = { 0 };
			av_strerror(re, buf, 1024);
			cout << "avcodec_send_packet failed: " << buf << endl;
			continue;
		}

		//接收解码后的数据，不占用CPU
		//一次send可能对应多次receive
		for (;;) {
			re = avcodec_receive_frame(cc, frame);
			if (re != 0) {
				break;
			}

			// 视频
			if (cc == vc) {
				vctx = sws_getCachedContext(
					vctx,	// 传NULL会新创建
					frame->width,
					frame->height,
					(AVPixelFormat)frame->format,	// 输入格式
					frame->width,
					frame->height,
					AV_PIX_FMT_RGBA,				// 输出格式
					SWS_BILINEAR,		//尺寸变换算法
					0,
					0,
					0
					);
				if (vctx) {
					cout << "像素尺寸转换上下文创建或获取成功" << endl;

					if (!rgb) {
						rgb = new unsigned char[frame->width * frame->height * 4];
					}

					uint8_t *data[2] = { 0 };
					data[0] = rgb;
					int lines[2] = {0};
					lines[0] = frame->width * 4;

					re = sws_scale(
						vctx,
						frame->data,		//输入数据
						frame->linesize,	//输入行大小
						0,
						frame->height,
						data,
						lines
					);

					cout << "sws_scale = " << re << endl;
				}
				else
					cout << "像素尺寸转换上下文创建或获取失败" << endl;
			}
			else {
				// 音频
				uint8_t *data[2] = { 0 };
				if (!pcm) {
					pcm = new uint8_t[frame->nb_samples * 2 * 2];
				}
				data[0] = pcm;

				re = swr_convert(actx,
					data,
					frame->nb_samples,
					(const uint8_t**)frame->data,
					frame->nb_samples);

				cout << "swr_convert = " << re << endl;
			}

			cout << "recv frame " << frame->format << " " << frame->linesize[0] << endl;
		}

	}

	av_frame_free(&frame);
	av_packet_free(&pkt);

	return 0;
}

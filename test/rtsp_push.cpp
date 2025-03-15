extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.h264> <rtsp_url>" << std::endl;
        return -1;
    }

    const char* inputFileName = argv[1];
    const char* rtspUrl = argv[2];

    // 初始化FFmpeg库
    av_register_all();
    avformat_network_init();

    // 打开输入文件
    AVFormatContext* inputFormatContext = nullptr;
    if (avformat_open_input(&inputFormatContext, inputFileName, nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file: " << inputFileName << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information." << std::endl;
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    // 创建输出RTSP流
    AVFormatContext* outputFormatContext = nullptr;
    avformat_alloc_output_context2(&outputFormatContext, nullptr, "rtsp", rtspUrl);
    if (!outputFormatContext) {
        std::cerr << "Could not create output context." << std::endl;
        avformat_close_input(&inputFormatContext);
        return -1;
    }

    // 添加输出流
    for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
        AVStream* inStream = inputFormatContext->streams[i];
        AVStream* outStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outStream) {
            std::cerr << "Failed to allocate output stream." << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return -1;
        }
        avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
        outStream->codecpar->codec_tag = 0;
    }

    // 打开输出流
    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputFormatContext->pb, outputFormatContext->filename, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output URL: " << rtspUrl << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return -1;
        }
    }

    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
        std::cerr << "Error occurred when opening output URL." << std::endl;
        avformat_close_input(&inputFormatContext);
        avformat_free_context(outputFormatContext);
        return -1;
    }

    // 获取视频流的帧率
    AVStream* videoStream = inputFormatContext->streams[0];
    double fps = av_q2d(videoStream->avg_frame_rate);
    if (fps <= 0) {
        fps = 30.0; // 默认帧率
    }
    std::cout << "推流帧率: " << fps << " FPS" << std::endl;

    // 推流循环
    AVPacket pkt;
    int64_t startTime = av_gettime(); // 推流开始时间
    while (true) {
        if (av_read_frame(inputFormatContext, &pkt) < 0) {
            // 文件读取结束，重新开始
            av_seek_frame(inputFormatContext, -1, 0, AVSEEK_FLAG_FRAME);
            continue;
        }

        if (pkt.stream_index < inputFormatContext->nb_streams) {
            AVStream* inStream = inputFormatContext->streams[pkt.stream_index];
            AVStream* outStream = outputFormatContext->streams[pkt.stream_index];

            // 计算时间戳
            pkt.pts = av_rescale_q_rnd(pkt.pts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.dts = av_rescale_q_rnd(pkt.dts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.duration = av_rescale_q(pkt.duration, inStream->time_base, outStream->time_base);
            pkt.pos = -1;

            // 控制推流速度
            int64_t ptsTime = av_rescale_q(pkt.pts, outStream->time_base, {1, 1000000});
            int64_t nowTime = av_gettime() - startTime;
            if (ptsTime > nowTime) {
                std::this_thread::sleep_for(std::chrono::microseconds(ptsTime - nowTime));
            }

            // 写入数据包
            if (av_interleaved_write_frame(outputFormatContext, &pkt) < 0) {
                std::cerr << "Error muxing packet." << std::endl;
                break;
            }
        }
        av_packet_unref(&pkt);
    }

    // 写入尾部信息并清理资源
    av_write_trailer(outputFormatContext);

    if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&outputFormatContext->pb);
    }

    avformat_free_context(outputFormatContext);
    avformat_close_input(&inputFormatContext);

    std::cout << "RTSP推流完成！" << std::endl;
    return 0;
}

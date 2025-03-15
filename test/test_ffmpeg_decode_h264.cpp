//
// Created by lining on 3/14/25.
//
#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <glog/logging.h>

#include <stdlib.h>
#include <string.h>

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <opencv2/opencv.hpp>


int decode_h264_pic(const std::string h264) {
    //1.初始化ffmpeg的解码器
    avcodec_register_all();
    AVCodec *pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!pCodec) {
        LOG(ERROR) << "Liveview " << "avcodec_find_decoder failed";
        return -1;
    }
    AVCodecParserContext *pCodecParserContext = av_parser_init(pCodec->id);
    if (!pCodecParserContext) {
        LOG(ERROR) << "Liveview " << "av_parser_init failed";
        return -2;

    }
    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext) {
        LOG(ERROR) << "Liveview " << "avcodec_alloc_context3 failed";
        return -3;
    }

    int32_t ret = avcodec_open2(pCodecContext, pCodec, NULL);
    if (ret < 0) {
        LOG(ERROR) << "Liveview " << "avcodec_open2 failed:ret=" << ret;
        return -4;
    }

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame) {
        LOG(ERROR) << "Liveview " << "av_frame_alloc failed";
        return -5;
    }
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG(ERROR) << "Liveview " << "av_packet_alloc failed";
        return -6;
    }

    SwsContext *pSwsCtx = nullptr;
    AVFrame *pFrameRGB = av_frame_alloc();
    if (pFrameRGB == nullptr) {
        LOG(ERROR) << "Liveview " << "av_frame_alloc failed";
        return -6;
    }

    FILE *file = fopen(h264.c_str(), "rb");
    if (file == nullptr) {
        LOG(ERROR) << "h264 open fail";
        return -7;
    }

    //不断的循环读取文件内容，直到读完
    uint8_t inbuf[1024 * 4] = {0};
    uint8_t *data = nullptr;
    int32_t data_size = 0;
    while (!feof(file)) {
        data_size = fread(inbuf, 1, 1024 * 4, file);
        while (data_size > 0) {
            data = inbuf;
            int processLen = av_parser_parse2(pCodecParserContext, pCodecContext, &packet->data, &packet->size,
                                              data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            data_size -= processLen;
            data += processLen;
            if (packet->size > 0) {
                printf("packet size:%d\n", packet->size);
                int gop = 0;
                avcodec_decode_video2(pCodecContext, pFrame, &gop, packet);
                if (gop) {
                    //将解码后的数据转换为图片
                    int w = pFrame->width;
                    int h = pFrame->height;
                    auto bufsize = avpicture_get_size(AV_PIX_FMT_RGB24, w, h);
                    auto rgbbuf = (uint8_t *) av_malloc(bufsize);


                    if (pSwsCtx == nullptr) {
                        sws_freeContext(pSwsCtx);
                        pSwsCtx = nullptr;
                    }
                    pSwsCtx = sws_getContext(w, h, pCodecContext->pix_fmt, w, h, AV_PIX_FMT_RGB24, 4, nullptr, nullptr,
                                             nullptr);
                    avpicture_fill((AVPicture *) pFrameRGB, rgbbuf, AV_PIX_FMT_RGB24, w, h);
                    sws_scale(pSwsCtx, (uint8_t const *const *) pFrame->data, pFrame->linesize, 0, pFrame->height,
                              pFrameRGB->data, pFrameRGB->linesize);
                    pFrameRGB->height = h;
                    pFrameRGB->width = w;

                    cv::Mat tmp(h, w, CV_8UC3, pFrameRGB->data[0], w * 3);
                    cv::cvtColor(tmp, tmp, cv::COLOR_RGB2BGR);
                    cv::imshow("1", tmp);
                    cv::waitKey(100);
                }
            }

        }

    }


}

static int32_t decode_packet(AVPacket *pkt, AVCodecContext *ctx, AVFrame *yuv, AVFrame *rgb,SwsContext *pSwsCtx) {
    int32_t result = 0;
    result = avcodec_send_packet(ctx, pkt);
    if (result < 0) {
        return -1;
    }
    while (result >= 0) {
        result = avcodec_receive_frame(ctx, yuv);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            return 1;
        } else if (result < 0) {
            return -1;
        }

        //将解码后的数据转换为图片
        int w = yuv->width;
        int h = yuv->height;
        auto bufsize = avpicture_get_size(AV_PIX_FMT_RGB24, w, h);
        auto rgbbuf = (uint8_t *) av_malloc(bufsize);


        if (pSwsCtx != nullptr) {
            sws_freeContext(pSwsCtx);
            pSwsCtx = nullptr;
        }
        pSwsCtx = sws_getContext(w, h, ctx->pix_fmt, w, h, AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr, nullptr,
                                 nullptr);
        avpicture_fill((AVPicture *) rgb, rgbbuf, AV_PIX_FMT_RGB24, w, h);
        sws_scale(pSwsCtx, (uint8_t const *const *) yuv->data, yuv->linesize, 0, yuv->height,
                  rgb->data, rgb->linesize);
        rgb->height = h;
        rgb->width = w;

        cv::Mat tmp(h, w, CV_8UC3, rgb->data[0], w * 3);
        cv::cvtColor(tmp, tmp, cv::COLOR_RGB2BGR);
        cv::imshow("1", tmp);
        cv::waitKey(300);
    }

    return 0;

}


int decode_h264_pic_new(const std::string h264) {
    //1.初始化ffmpeg的解码器
    avcodec_register_all();
    AVCodec *pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!pCodec) {
        LOG(ERROR) << "Liveview " << "avcodec_find_decoder failed";
        return -1;
    }
    AVCodecParserContext *pCodecParserContext = av_parser_init(pCodec->id);
    if (!pCodecParserContext) {
        LOG(ERROR) << "Liveview " << "av_parser_init failed";
        return -2;

    }
    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext) {
        LOG(ERROR) << "Liveview " << "avcodec_alloc_context3 failed";
        return -3;
    }

    int32_t ret = avcodec_open2(pCodecContext, pCodec, NULL);
    if (ret < 0) {
        LOG(ERROR) << "Liveview " << "avcodec_open2 failed:ret=" << ret;
        return -4;
    }

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame) {
        LOG(ERROR) << "Liveview " << "av_frame_alloc failed";
        return -5;
    }
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG(ERROR) << "Liveview " << "av_packet_alloc failed";
        return -6;
    }

    SwsContext *pSwsCtx = nullptr;
    AVFrame *pFrameRGB = av_frame_alloc();
    if (pFrameRGB == nullptr) {
        LOG(ERROR) << "Liveview " << "av_frame_alloc failed";
        return -6;
    }

    FILE *file = fopen(h264.c_str(), "rb");
    if (file == nullptr) {
        LOG(ERROR) << "h264 open fail";
        return -7;
    }

    //不断的循环读取文件内容，直到读完
    uint8_t inbuf[1024 * 8] = {0};
    uint8_t *data = nullptr;
    int32_t data_size = 0;
    while (!feof(file)) {
        data_size = fread(inbuf, 1, 1024 * 8, file);
        while (data_size > 0) {
            data = inbuf;
            int processLen = av_parser_parse2(pCodecParserContext, pCodecContext, &packet->data, &packet->size,
                                              data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            data_size -= processLen;
            data += processLen;
            if (packet->size > 0) {
                printf("packet size:%d\n", packet->size);
                decode_packet(packet,pCodecContext,pFrame,pFrameRGB,pSwsCtx);
            }

        }

    }


}


int test_rtsp(){
    av_register_all();
    avformat_network_init();

    const char *input_filename = "video/test.264";
    const char *output_url = "rtsp://localhost/live/test";

    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVOutputFormat *ofmt = NULL;
    AVPacket pkt;

    int ret, i;
    int video_index = -1;

    // 打开输入文件
    if ((ret = avformat_open_input(&ifmt_ctx, input_filename, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", input_filename);
//        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
//        goto end;
    }

    // 查找视频流
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
            break;
        }
    }

    if (video_index == -1) {
        fprintf(stderr, "Could not find video stream");
//        goto end;
    }

    // 分配输出上下文
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", output_url);
    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context");
        ret = AVERROR_UNKNOWN;
//        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    // 添加视频流到输出上下文
    AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "Failed to allocate output stream");
        ret = AVERROR_UNKNOWN;
//        goto end;
    }

    AVCodecParameters *in_codecpar = ifmt_ctx->streams[video_index]->codecpar;
    ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy codec parameters");
//        goto end;
    }
    out_stream->codecpar->codec_tag = 0;

    // 打开输出URL
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output URL '%s'", output_url);
//            goto end;
        }
    }

    // 写文件头
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output URL");
//        goto end;
    }
    static int64_t pts = 0;
    // 读取并写入数据包
    while (1) {
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            // 重新定位到文件开头
            av_seek_frame(ifmt_ctx, video_index, 0, AVSEEK_FLAG_BACKWARD);
            continue;
        }

        if (pkt.stream_index == video_index) {
            pkt.pts = pts;
            pkt.dts = pts;
            pts += pkt.duration; // 根据帧率递增时间戳
        }

        // 转换时间戳并写入输出
        pkt.pts = av_rescale_q_rnd(pkt.pts, ifmt_ctx->streams[video_index]->time_base, out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, ifmt_ctx->streams[video_index]->time_base, out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, ifmt_ctx->streams[video_index]->time_base, out_stream->time_base);

        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
//            fprintf(stderr, "Error muxing packet: %s", av_err2str(ret));
            break;
        }

        av_packet_unref(&pkt);
    }

    // 写文件尾
    av_write_trailer(ofmt_ctx);

//    end:
//    if (ifmt_ctx)
//        avformat_close_input(&ifmt_ctx);
//
//    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
//        avio_closep(&ofmt_ctx->pb);
//
//    avformat_free_context(ofmt_ctx);
//
//    if (ret < 0 && ret != AVERROR_EOF) {
//        fprintf(stderr, "Error occurred: %s", av_err2str(ret));
//        return 1;
//    }

    return 0;
}


int main() {

//    decode_h264_pic_new("./video/test.264");

    test_rtsp();

    return 0;
}
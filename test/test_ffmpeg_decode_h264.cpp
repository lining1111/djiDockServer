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



int main() {
    decode_h264_pic_new("./video/test.264");

    return 0;
}
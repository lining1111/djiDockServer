//
// Created by lining on 3/15/25.
//

#ifndef ENCODERIMAGE_H
#define ENCODERIMAGE_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

#include <opencv2/opencv.hpp>

#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <glog/logging.h>

class EncoderImage {
private:
    std::string _name;
    AVFrame *pFrameYUV = nullptr;
    AVFrame *pFrameRGB = nullptr;
public:
    EncoderImage(const std::string &name) {
        _name = name;
    }

    ~EncoderImage() {

    }

    int init() {
        pFrameYUV = av_frame_alloc();
        pFrameRGB = av_frame_alloc();
        if (pFrameYUV == nullptr || pFrameRGB == nullptr) {
            LOG(ERROR) << "Liveview " << _name << " can not alloc frame";
            return -1;
        }
        return 0;
    }

    void deInit() {
        if (pFrameYUV != nullptr) {
            av_free(pFrameYUV);
        }
        if (pFrameRGB != nullptr) {
            av_free(pFrameRGB);
        }
    }

    void encode(AVCodecContext *ctx, AVPacket *packet, cv::Mat &image) {
        //图片编码器相关
        SwsContext *pSwsCtx = nullptr;
        uint8_t *rbgBuf = nullptr;
        size_t bufSize = 0;

        //开始编码
        int32_t result = 0;
        result = avcodec_send_packet(ctx, packet);
        if (result < 0) {
            LOG(ERROR) << "Liveview " << _name << " can not send packet";
            return;
        }

        if (pFrameYUV == nullptr || pFrameRGB == nullptr) {
            LOG(ERROR) << "Liveview " << _name << " frame null";
            return;
        }

        while (result >= 0) {
            result = avcodec_receive_frame(ctx, pFrameYUV);
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                break;
            } else if (result < 0) {
                LOG(ERROR) << "Liveview " << _name << " can not decode packet";
                break;
            }

            //将解码后的数据编码成图片
            int w = pFrameYUV->width;
            int h = pFrameYUV->height;
            bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
            if (rbgBuf == nullptr) {
                rbgBuf = (uint8_t *) av_malloc(bufSize * sizeof(uint8_t));
            }

            if (pSwsCtx != nullptr) {
                sws_freeContext(pSwsCtx);
                pSwsCtx = nullptr;
            }
            pSwsCtx = sws_getContext(w, h, AV_PIX_FMT_YUV420P, w, h, AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr, nullptr,
                                     nullptr);
            if (pSwsCtx == nullptr) {
                LOG(ERROR) << "Liveview " << _name << " can not alloc sws context";
                return;
            }

            av_image_fill_arrays( pFrameRGB->data,pFrameRGB->linesize, rbgBuf, AV_PIX_FMT_RGB24, w, h,1);
            sws_scale(pSwsCtx, (uint8_t const *const *) pFrameYUV->data, pFrameYUV->linesize, 0, h, pFrameRGB->data,
                      pFrameRGB->linesize);
            pFrameRGB->width = w;
            pFrameRGB->height = h;
            cv::Mat tmp(h, w, CV_8UC3, pFrameRGB->data[0], w * 3);
            cv::cvtColor(tmp, image, cv::COLOR_RGB2BGR);
        }

        if (pSwsCtx != nullptr) {
            sws_freeContext(pSwsCtx);
            pSwsCtx = nullptr;
        }
        if (rbgBuf != nullptr) {
            av_free(rbgBuf);
        }

    }


};


#endif //ENCODERIMAGE_H

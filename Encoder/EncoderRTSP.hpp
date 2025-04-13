//
// Created by lining on 3/15/25.
//

#ifndef ENCODERRTSP_H
#define ENCODERRTSP_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}


#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <glog/logging.h>
#include <map>


class EncoderRTSP {
    typedef struct StreamInfo {
        int w;
        int h;
        int fps;
    } StreamInfo;

    std::map<int, StreamInfo> streamInfoMap = {
            {1, {960, 540, 30}},
            {2, {1280, 720, 30}},
            {3, {1280, 720, 30}},
            {4, {1920, 1080, 30}},
            {5, {1920, 1080, 30}},
    };


private:
    std::string  _name;
    int _quality;
    //RTSP流编码器相关
    AVFormatContext *pFormatCtx = nullptr;

    public:
    EncoderRTSP(const std::string &rtspAddr,int quality){
        _name = rtspAddr;
        _quality = quality;
    }
    ~EncoderRTSP() {

    }

    int init() {
        int ret = avformat_alloc_output_context2(&pFormatCtx, nullptr, "rtsp", _name.c_str());
        if (ret < 0) {
            LOG(ERROR) << "Liveview " << _name << " can not alloc output context";
            return -1;
        }

        //配置输出流
        AVStream *out_stream = avformat_new_stream(pFormatCtx, NULL);
        if (out_stream == nullptr) {
            LOG(ERROR) << "Liveview " << _name << " can not alloc output stream";
            return -1;
        }

        out_stream->codecpar->codec_id = AV_CODEC_ID_H264;
        out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        //根据初始化时的视频质量来设置参数
        auto iter = streamInfoMap[_quality];
        out_stream->codecpar->width = iter.w;
        out_stream->codecpar->height = iter.h;
        out_stream->time_base = AVRational{1, iter.fps};

        if (!(pFormatCtx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&pFormatCtx->pb, _name.c_str(), AVIO_FLAG_WRITE) < 0) {
                LOG(ERROR) << "Liveview " << _name << " can not open rtsp";
                return -1;
            }
        }
        if (avformat_write_header(pFormatCtx, NULL) < 0) {
            LOG(ERROR) << "Liveview " << _name << " can not write header";
            return -1;
        }

        return 0;
    }

    void deInit(){
        if (pFormatCtx != nullptr) {
            if (!(pFormatCtx->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&pFormatCtx->pb);
            }
            avformat_free_context(pFormatCtx);
        }

    }

    void encode(AVPacket *packet) {
        if (av_interleaved_write_frame(pFormatCtx, packet) < 0) {
            LOG(ERROR) << "Liveview " << _name << " can not write frame";
        }
    }

};


#endif //ENCODERRTSP_H

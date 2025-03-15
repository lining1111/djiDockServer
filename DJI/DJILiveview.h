//
// Created by lining on 3/12/25.
//

#ifndef DJILIVEVIEW_H
#define DJILIVEVIEW_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

#include <opencv2/opencv.hpp>

#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <glog/logging.h>

#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <map>
#include <condition_variable>
#include <future>
#include "liveview.h"

namespace dji {
    class DJILiveview {
        typedef struct StreamInfo {
            int w;
            int h;
            int fps;
        } StreamInfo;

        std::map<edge_sdk::Liveview::StreamQuality, StreamInfo> streamInfoMap = {
                {edge_sdk::Liveview::kStreamQuality540p, {960, 540, 30}},
                {edge_sdk::Liveview::kStreamQuality720p, {1280, 720, 30}},
                {edge_sdk::Liveview::kStreamQuality720pHigh, {1280, 720, 30}},
                {edge_sdk::Liveview::kStreamQuality1080p, {1920, 1080, 30}},
                {edge_sdk::Liveview::kStreamQuality1080pHigh, {1920, 1080, 30}},
        };


    private:
        std::string _name;
        edge_sdk::Liveview::CameraType _type;
        edge_sdk::Liveview::StreamQuality _quality;
        bool isEncodeImage = false;
        std::shared_ptr<edge_sdk::Liveview> _liveview = nullptr;
        edge_sdk::Liveview::LiveviewStatus _status;
        std::atomic<uint32_t> _stream_bitrate_kbps;
        std::chrono::system_clock::time_point _recv_stream_data_time = std::chrono::system_clock::now();
        uint32_t _recv_stream_data_total_size = 0;
        //h264缓存相关
        std::vector<uint8_t> _h264_cache;
        std::mutex _h264_cache_mutex;
        std::condition_variable _h264_cache_cv;

        bool _isRun = false;
        bool isLocalThreadRun = false;
        std::shared_future<int> f_startStream;
        std::shared_future<int> f_procStream;
        std::thread t_procStream;

    public:
        DJILiveview(const std::string name);

    public:
        /**
         * 初始化实时流
         * @param type 摄像头种类 0:FPV Camera 1:Payload Camera
         * @param quality 视频质量  1:540p: 30fps, 960*540, bps 512*1024
         *                          2:720p: 30fps, 1280*720, bps 1024*1024
         *                          3:720pHigh: 30fps, 1280*720, bps 1024*1024 + 512*1024
         *                          4:1080p: 30fps, 1920*1080, bps 3*1024*1024
         *                          5:1080p: 30fps, 1920*1080, bps 8*1024*1024
         * @return
         */
        edge_sdk::ErrorCode Init(edge_sdk::Liveview::CameraType type, edge_sdk::Liveview::StreamQuality quality);

        /**
         * 开始实时流
         * @return
         */
        edge_sdk::ErrorCode Start();


        edge_sdk::ErrorCode Stop();

        /**
         * 设置输入源
         * @param source 输入源 1:Wide-angle lens 2:Zoom lens 3:Infrared lens
         * @return
         */
        edge_sdk::ErrorCode SetCameraSource(edge_sdk::Liveview::CameraSource source);


    public:
        uint32_t GetStreamBitrate() const {
            return _stream_bitrate_kbps.load();
        }

    private:

        //与获取原始流数据H264相关的，ThreadProcessH264Stream 是处理数据流的线程，可以按需定制
        /**
         * 获取裸流H264数据的回调
         * @param data
         * @param len
         * @return
         */
        edge_sdk::ErrorCode StreamCallback(const uint8_t *data, size_t len);

        /**
         * 实时预览流状态回调
         * @param status
         */
        void LiveviewStatusCallback(const edge_sdk::Liveview::LiveviewStatus &status);

        /**
         * h264裸流处理流程
         * @param local
         */
        static int ThreadProcessH264Stream(DJILiveview *local);

        /**
         * 如果状态是0.则一直发送开始推流命令
         * @param local
         */
        static int ThreadStartH264Stream(DJILiveview *local);

        /**
        * 停止实时流
        * @return
        */
        edge_sdk::ErrorCode StopH264Stream();


        //基于ffmpeg的编解码相关
        //解码器 data-->AVPacket-->AVFrame
        AVCodec *pCodec = nullptr;
        AVCodecContext *pCodecCtx = nullptr;
        AVCodecParserContext *pCodecParserCtx = nullptr;
        AVPacket *pPacket = nullptr;

        int initDecoder();

        void deInitDecoder();

        void decoding(const uint8_t *data, size_t len);

        //编码相关的，分为编码到图片、编码到RTSP流
        //编码器 AVFrame-->AVPacket-->data
        /**
         * 一直解码，可以是图片，可以是RTSP流
         */

        AVFrame *pFrameYUV = nullptr;
        AVFrame *pFrameRGB = nullptr;

        int initImageEncoder() {
            pFrameYUV = av_frame_alloc();
            pFrameRGB = av_frame_alloc();
            if (pFrameYUV == nullptr || pFrameRGB == nullptr) {
                LOG(ERROR) << "Liveview " << _name << " can not alloc frame";
                return -1;
            }
            return 0;
        }

        void deInitImageEncoder() {
            if (pFrameYUV != nullptr) {
                av_free(pFrameYUV);
            }
            if (pFrameRGB != nullptr) {
                av_free(pFrameRGB);
            }
        }

        /**
         * 编码到图片
         * @param frame
         * @param image
         */
        void encodeToImage(AVPacket *packet, cv::Mat &image);

        //RTSP流编码器相关
        AVFormatContext *pFormatCtx = nullptr;

        int initRTSPEncoder(std::string rtsp_addr) {
            int ret = avformat_alloc_output_context2(&pFormatCtx, nullptr, "rtsp", rtsp_addr.c_str());
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
                if (avio_open(&pFormatCtx->pb, rtsp_addr.c_str(), AVIO_FLAG_WRITE) < 0) {
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

        void deInitRTSPEncoder() {
            if (pFormatCtx != nullptr) {
                if (!(pFormatCtx->oformat->flags & AVFMT_NOFILE)) {
                    avio_closep(&pFormatCtx->pb);
                }
                avformat_free_context(pFormatCtx);
            }
        }

        //编码器
        void encodeToRTSP(AVPacket *packet);

    };
}


#endif //DJILIVEVIEW_H

//
// Created by lining on 3/12/25.
//

#ifndef DJILIVEVIEW_H
#define DJILIVEVIEW_H

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
#include "Encoder/EncoderRTSP.hpp"
#include "Encoder/EncoderImage.hpp"


namespace dji {
    class DJILiveview {
    private:
        std::string _name;
        edge_sdk::Liveview::CameraType _type = edge_sdk::Liveview::kCameraTypePayload;
        edge_sdk::Liveview::StreamQuality _quality = edge_sdk::Liveview::kStreamQuality540p;
        bool isEncodeImage = false;
        std::shared_ptr<edge_sdk::Liveview> _liveview = nullptr;
        edge_sdk::Liveview::LiveviewStatus _status;
        std::atomic<uint32_t> _stream_bitrate_kbps = {0};
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
        EncoderImage *encoderImage = nullptr;
        EncoderRTSP *encoderRTSP = nullptr;

    };
}


#endif //DJILIVEVIEW_H

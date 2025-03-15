//
// Created by lining on 3/12/25.
//

#include <thread>

#include "DJILiveview.h"

#define BITRATE_CALCULATE_BITS_PER_BYTE 8
#define BITRATE_CALCULATE_INTERVAL_TIME_MS 2000

namespace dji {
    DJILiveview::DJILiveview(const std::string name) {
        _name = name;
        _liveview = edge_sdk::CreateLiveview();
        _status = 0;
    }

    edge_sdk::ErrorCode DJILiveview::Start() {
        _isRun = true;
        isLocalThreadRun = true;
        //1.开启流处理流程
        f_procStream = std::async(std::launch::async, &DJILiveview::ThreadProcessH264Stream, this);
        //2.开启一个线程，内容是如果_status是0 就一直发送开启推流
        f_startStream = std::async(std::launch::async, &DJILiveview::ThreadStartH264Stream, this);
        return edge_sdk::kOk;
    }

    edge_sdk::ErrorCode DJILiveview::Stop() {
        _isRun = false;
        if (isLocalThreadRun) {
            isLocalThreadRun = false;
            _h264_cache.push_back(1);
            _h264_cache_cv.notify_one();
            try {
                f_procStream.wait();
            } catch (std::exception &e) {
                LOG(ERROR) << e.what();
            }
        }

        auto rc = _liveview->StopH264Stream();
        if (rc != edge_sdk::kOk) {
            LOG(ERROR) << "StopH264Stream failed, rc = " << rc;
            return rc;
        }

        return edge_sdk::kOk;
    }

    edge_sdk::ErrorCode
    DJILiveview::Init(edge_sdk::Liveview::CameraType type, edge_sdk::Liveview::StreamQuality quality) {
        auto stream_callback = std::bind(&DJILiveview::StreamCallback, this, std::placeholders::_1,
                                         std::placeholders::_2);
        _type = type;
        _quality = quality;
        edge_sdk::Liveview::Options options = {
                .camera = type,
                .quality = quality,
                .callback = stream_callback,
        };

        auto rc = _liveview->Init(options);

        _liveview->SubscribeLiveviewStatus(
                std::bind(&DJILiveview::LiveviewStatusCallback, this, std::placeholders::_1));

        return rc;
    }

    edge_sdk::ErrorCode DJILiveview::SetCameraSource(edge_sdk::Liveview::CameraSource source) {
        return _liveview->SetCameraSource(source);
    }

    edge_sdk::ErrorCode DJILiveview::StreamCallback(const uint8_t *data, size_t len) {
        auto now = std::chrono::system_clock::now();
        //1.将数据写入缓存中
        std::lock_guard<std::mutex> lock(_h264_cache_mutex);
        _h264_cache.insert(_h264_cache.end(), data, data + len);
        _h264_cache_cv.notify_one();
//        LOG(INFO) << "recv h264 data len:" << len;
        //2.计算视频的码率
        _recv_stream_data_total_size += len;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _recv_stream_data_time).count();
        if (duration >= BITRATE_CALCULATE_BITS_PER_BYTE) {
            auto kbps = _recv_stream_data_total_size * BITRATE_CALCULATE_BITS_PER_BYTE /
                        (BITRATE_CALCULATE_INTERVAL_TIME_MS / 1000) / 1024;
            _stream_bitrate_kbps = kbps;
            _recv_stream_data_total_size = 0;
            _recv_stream_data_time = now;
        }

        return edge_sdk::kOk;
    }


    static std::string getLiveviewStatus(edge_sdk::Liveview::LiveviewStatus &status) {
        //直播码流状态，每一个位代表不同码流质量的有效状态，
        // bit0：表示自适应码流是否有效，
        // bit1：表示540p码流是否有效，
        // bit2：表示720p码流是否有效，
        // bit3：表示720pHigh码流是否有效，
        // bit4：表示1080p码流是否有效，
        // bit5：表示1080pHigh 码流是否有效
        std::string stream[6] = {"Adaptive", "540p", "720p", "720pHigh", "1080p", "1080pHigh"};
        std::string ret;
        for (int i = 0; i < 6; i++) {
            if (status & (1 << i)) {
                ret += stream[i] + "可用,";
            } else {
                ret += stream[i] + "不可用,";
            }
        }
        return ret;
    }

    void DJILiveview::LiveviewStatusCallback(const edge_sdk::Liveview::LiveviewStatus &status) {
        _status = status;
        LOG(INFO) << "Liveview " << _name << " status: " << status << ", " << getLiveviewStatus(_status);
    }

    int DJILiveview::ThreadProcessH264Stream(DJILiveview *local) {
        LOG(WARNING) << "Liveview " << local->_name << "ThreadProcessH264Stream start";
        std::string threadName = local->_name + "_streamdecoder";
        pthread_setname_np(pthread_self(), threadName.c_str());
        {
            sched_param sch;
            int policy;
            pthread_getschedparam(pthread_self(), &policy,
                                  &sch);
            sch.sched_priority = 40;
            if (pthread_setschedparam(pthread_self(),
                                      SCHED_FIFO, &sch)) {
                LOG(ERROR) << "Liveview " << local->_name << "Failed to setschedparam:" << strerror(errno);
            }
        }
        //1.初始化ffmpeg的解码器
        local->initDecoder();
        if (local->isEncodeImage) {
            //1.1初始化图片编码器
            local->encoderImage = new EncoderImage(local->_name);
            local->encoderImage->init();
        } else {
            //1.2初始化RTSP编码器
            local->encoderRTSP = new EncoderRTSP("rtsp://192.168.200.251/live/test", local->_quality);
            local->encoderRTSP->init();
        }

        //2.持续将接到的流解析成图片
        std::vector<uint8_t> decode_data;
        while (local->_isRun) {
            //2.1拿到锁之后将数据整体的写入本地缓存中
            std::unique_lock<std::mutex> lock(local->_h264_cache_mutex);
            local->_h264_cache_cv.wait(lock, [local]() { return local->_h264_cache.size() != 0; });
            decode_data = local->_h264_cache;
            local->_h264_cache.clear();
            lock.unlock();
            //2.2将本地缓存放入解码
            local->decoding(decode_data.data(), decode_data.size());
        }
        //3.释放资源
        local->deInitDecoder();
        if (local->encoderImage != nullptr) {
            local->encoderImage->deInit();
            delete local->encoderImage;
            local->encoderImage = nullptr;
        }
        if (local->encoderRTSP != nullptr) {
            local->encoderRTSP->deInit();
            delete local->encoderRTSP;
            local->encoderRTSP = nullptr;
        }
        LOG(WARNING) << "Liveview " << local->_name << "ThreadProcessH264Stream end";
        return 0;
    }


    int DJILiveview::ThreadStartH264Stream(DJILiveview *local) {
        LOG(WARNING) << "Liveview " << local->_name << "ThreadStartH264Stream start";
        while (local->_isRun) {
            while (local->_status == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
//            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            auto rc = local->_liveview->StartH264Stream();
            if (rc != edge_sdk::kOk) {
                LOG(ERROR) << "Liveview " << local->_name << " StartH264Stream error " << rc;
            } else {
                LOG(INFO) << "Liveview " << local->_name << " StartH264Stream success";
                break;
            }

        }

        LOG(WARNING) << "Liveview " << local->_name << "ThreadStartH264Stream end";
        return 0;
    }

    edge_sdk::ErrorCode DJILiveview::StopH264Stream() {
        return _liveview->StopH264Stream();
    }

    int DJILiveview::initDecoder() {
        pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!pCodec) {
            LOG(ERROR) << "Liveview " << _name << " can not find codec H264,try cmd 'ffmpeg -codecs | grep h264'";
            return -1;
        }
        pCodecParserCtx = av_parser_init(pCodec->id);
        if (!pCodecParserCtx) {
            LOG(ERROR) << "Liveview " << _name << " can not find codec parser";
            return -2;
        }
        pCodecCtx = avcodec_alloc_context3(pCodec);
        if (!pCodecCtx) {
            LOG(ERROR) << "Liveview " << _name << " can not alloc codec context";
            return -3;
        }
        pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        pCodecCtx->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;

        if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
            LOG(ERROR) << "Liveview " << _name << " can not open codec";
            return -4;
        }
        //初始化包和帧
        pPacket = av_packet_alloc();
        if (!pPacket) {
            LOG(ERROR) << "Liveview " << _name << " can not alloc packet";
            return -5;
        }

        return 0;
    }

    void DJILiveview::deInitDecoder() {
        av_parser_close(pCodecParserCtx);
        avcodec_free_context(&pCodecCtx);
        av_packet_free(&pPacket);
    }


    void DJILiveview::decoding(const uint8_t *data, size_t len) {
        if (pCodecCtx == nullptr) {
            LOG(ERROR) << "Liveview " << _name << " codec context is null";
            return;
        }

        int remain = len;
        uint8_t *dataPtr = (uint8_t *) data;
        while (remain > 0) {
            int procLen = av_parser_parse2(pCodecParserCtx, pCodecCtx, &pPacket->data, &pPacket->size,
                                           dataPtr, remain, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
            if (procLen < 0) {
                LOG(ERROR) << "Liveview " << _name << " can not parse packet";
                return;
            }

            remain -= procLen;
            dataPtr += procLen;
            if (pPacket->size > 0) {
                VLOG(2) << "Liveview " << _name << " packet size " << pPacket->size;
                if (isEncodeImage) {
                    //解析成图片
                    cv::Mat image;
                    if (encoderImage != nullptr) {
                        encoderImage->encode(pCodecCtx, pPacket, image);
                        LOG(INFO) << "imag size:h:" << image.rows << ",w:" << image.cols;
//                    cv::imshow("image", image);
//                    cv::waitKey(1);
                    }
                } else {
                    if (encoderRTSP != nullptr) {
                        encoderRTSP->encode(pPacket);
                    }
                }

            }
        }

    }


}
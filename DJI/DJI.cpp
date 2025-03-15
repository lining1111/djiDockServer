//
// Created by lining on 3/12/25.
//

#include "DJI.h"

#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <glog/logging.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "../utils/utils.h"
#include <dirent.h>

namespace dji {

    std::queue<edge_sdk::MediaFile> media_file_queue;
    std::mutex media_file_queue_mutex;
    std::condition_variable media_file_queue_cv;

    DJI::DJI(edge_sdk::Options options) : _options(options) {
    }

    DJI::~DJI() {
        _isRun = false;
    }

    edge_sdk::ErrorCode DJI::InitSDK() {
        auto rc = edge_sdk::ESDKInit::Instance()->Init(_options);
        if (rc != edge_sdk::kOk) {
            LOG(ERROR) << "Failed to init SDK, error code: " << rc;
        }
        return rc;
    }

    edge_sdk::ErrorCode DJI::DeInitSDK() {
        return edge_sdk::ESDKInit::Instance()->DeInit();
    }

    edge_sdk::ErrorCode DJI::InitGetMediaFiles() {
        //1.初始化媒体文件管理器
        _mediaManager = edge_sdk::MediaManager::Instance();
        if (_mediaManager == nullptr) {
            LOG(ERROR) << "Failed to init media manager";
            return edge_sdk::kErrorNullPointer;
        }
        //2.设置无人机巢自动删除为否
        auto rc = _mediaManager->SetDroneNestAutoDelete(false);
        if (rc != edge_sdk::kOk) {
            LOG(ERROR) << "Failed to set drone nest auto delete, error code: " << rc;
            return rc;
        }

        //3.创建媒体文件读取器
        _mediaFilesReader = _mediaManager->CreateMediaFilesReader();
        rc = _mediaFilesReader->Init();
        if (rc != edge_sdk::kOk) {
            LOG(ERROR) << "Failed to init media files reader, error code: " << rc;
            return rc;
        }

        //4.注册接收机场媒体文件更新通知
        _mediaManager->RegisterMediaFilesObserver(std::bind(MediaFilesUpdateCallback, std::placeholders::_1));


        auto t = std::thread(&ProcessMediaFileThread, this);
        t.detach();

        return edge_sdk::kOk;
    }


    static string convertTimeStamp2TimeStr(time_t timeStamp) {
        struct tm *timeinfo = nullptr;
        char buffer[80];
        timeinfo = localtime(&timeStamp);
        strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
        printf("%s\n", buffer);
        return string(buffer);
    }

    static string CameraType(edge_sdk::MediaFile::CameraAttr ca) {
        if (ca == edge_sdk::MediaFile::kCameraAttrInfrared) {
            return "热成像相机";
        } else if (ca == edge_sdk::MediaFile::kCameraAttrZoom) {
            return "可变焦距相机";
        } else if (ca == edge_sdk::MediaFile::kCameraAttrWide) {
            return "广角相机";
        } else if (ca == edge_sdk::MediaFile::kCameraAttrVisible) {
            return "可视相机";
        } else {
            return "未知相机" + to_string(ca);
        }
    }

    edge_sdk::ErrorCode DJI::MediaFilesUpdateCallback(const edge_sdk::MediaFile &file) {
        LOG(INFO) << "有来自摄像头:" << CameraType(file.camera_attr) << "的新的媒体文件:" << file.file_name << ",path:"
                  << file.file_path;
        LOG(INFO) << "文件信息:大小:" << file.file_size << "类型:" << file.file_type << "时间:"
                  << convertTimeStamp2TimeStr(file.create_time)
                  << "经纬度:" << to_string(file.longitude) << "," << to_string(file.latitude);
        std::lock_guard<std::mutex> l(media_file_queue_mutex);
        media_file_queue.push(file);
        media_file_queue_cv.notify_one();
        return edge_sdk::kOk;
    }

    edge_sdk::ErrorCode DJI::ReadMediaFile(const edge_sdk::MediaFile &file,
                                           std::vector<uint8_t> &image) {
        char buf[1024 * 1024];
        auto fd = _mediaFilesReader->Open(file.file_path);
        if (fd < 0) {
            return edge_sdk::kErrorSystemError;
        }
        do {
            auto nread = _mediaFilesReader->Read(fd, buf, sizeof(buf));
            if (nread > 0) {
                image.insert(image.end(), (uint8_t *) buf, (uint8_t *) (buf + nread));
            } else {
                _mediaFilesReader->Close(fd);
                break;
            }
        } while (1);

        INFO("filesize: %d, image.size: %d", file.file_size, image.size());

        return edge_sdk::kOk;
    }

    static std::string GetMediaFileSavePath() {
        //1.需要按日期写入到不同的目录下
        //获取当天的日期
        time_t now = time(0); // 获取当前时间的秒数
        tm *ltm = localtime(&now); // 转换为tm结构体
        string date = to_string(1900 + ltm->tm_year) + "_" + to_string(1 + ltm->tm_mon) + "_" + to_string(ltm->tm_mday);
        return "media/" + date + "/";
    }

    static void DumpMediaFile(const std::string &filename,
                              const std::vector<uint8_t> &data,const std::string &savePath) {
        if (access(savePath.c_str(), 0) == -1) {
            CreatePath(savePath);
        }
        std::string file_name = savePath + filename;
        FILE *f = fopen(file_name.c_str(), "wb");
        if (f) {
            fwrite(data.data(), data.size(), 1, f);
            fclose(f);
        }
    }

    static int UploadMediaFileInfo(const std::string &filename, const std::string &savePath) {
        //TODO 按照通信格式将新的文件信息上传到服务器
        return 0;
    }


    void DJI::ProcessMediaFileThread(DJI *local) {
        std::vector<uint8_t> image;

        LOG(WARNING) << "ProcessMediaFileThread start";
        while (local->_isRun) {

            std::unique_lock<std::mutex> l(media_file_queue_mutex);

            media_file_queue_cv.wait(l, [] { return media_file_queue.size() != 0; });

            edge_sdk::MediaFile file = media_file_queue.front();
            media_file_queue.pop();
            l.unlock();

            LOG(INFO) << "process media file:" << file.file_name;
            auto rc = local->ReadMediaFile(file, image);
            if (rc == edge_sdk::kOk) {
                std::string savePath = GetMediaFileSavePath();
                DumpMediaFile(file.file_name, image,savePath);
                if (UploadMediaFileInfo(file.file_name, savePath) == 0) {
                    LOG(INFO) << "upload media file  info success:" << file.file_name;
                }else{
                    LOG(ERROR) << "upload media file  info failed:" << file.file_name;
                }
            } else {
                LOG(ERROR) << "dump media file failed:" << file.file_name.c_str();
            }

            image.clear();
        }
        LOG(WARNING) << "ProcessMediaFileThread exit";

    }
}
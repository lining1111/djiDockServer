//
// Created by lining on 3/12/25.
//

#ifndef DJI_H
#define DJI_H

#include <string>
#include "init/esdk_init.h"
#include "media_manager.h"
#include <vector>

using namespace std;

/**
 * 大疆机场的拉流服务 基于Edge-SDK https://developer.dji.com/doc/edge-sdk-tutorial/cn/
 */

namespace dji {
    class DJI {
    private:
        edge_sdk::Options _options;
        //获取最近的航线文件相关
        bool _isRun = false;
        edge_sdk::MediaManager *_mediaManager = nullptr;
        std::shared_ptr<edge_sdk::MediaFilesReader> _mediaFilesReader = nullptr;

    public:
        DJI(edge_sdk::Options options);
        ~DJI();

    public:
        edge_sdk::ErrorCode InitSDK();

        edge_sdk::ErrorCode DeInitSDK();

        /**
         * 获取最近的航线文件 初始化
         * @return
         */
        edge_sdk::ErrorCode InitGetMediaFiles();

    private:

        /**
         * 机场有文件更新时的处理回调，读取文件信息到待处理队列
         * @param file
         * @return
         */
        static edge_sdk::ErrorCode MediaFilesUpdateCallback(const edge_sdk::MediaFile &file);

        /**
         * 队列有新文件时，拉取到本地
         */
        static void ProcessMediaFileThread(DJI *local);


        edge_sdk::ErrorCode ReadMediaFile(const edge_sdk::MediaFile& file,
                                                 std::vector<uint8_t>& image);


    };

}


#endif //DJI_H

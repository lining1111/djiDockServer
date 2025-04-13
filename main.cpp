#include <iostream>
#include "DJI/DJI.h"
#include "DJI/DJILiveview.h"
#include "glogHelper/GlogHelper.h"
#include "utils/utils.h"
#include <gflags/gflags.h>
#include "DJI/KeyStoreDefault.h"

static edge_sdk::ErrorCode PrintConsoleFunc(const uint8_t *data, uint16_t dataLen) {
    printf("%s", data);
    return edge_sdk::kOk;
}

void InitOptions(edge_sdk::Options &options) {
    options.product_name = "Edge-1.0";
    options.vendor_name = "Vendor";
    options.serial_number = "SN0000100010101";
    options.firmware_version = {0, 1, 0, 0};
    edge_sdk::AppInfo app_info;
    app_info.app_name = "JVLUTEST";
    app_info.app_id = "159314";
    app_info.app_key = "99263a8cd88d751e89827fc28e9f3ca";
    app_info.app_license = "ZIVlYbw4eVTUHk6MMEXBRafqw8nPC2uLiB9O7zroRlLn3gJNgladUfFhmWMrrLwz/iHwbhic27FUOdRlVJMdQkR40tIRTAqNKqkSEDiBQpcE3e5iiBzSqlCFyIsttHNaMVTD4emLoUNR6O5DI4KTAxFo7HF5P0YeVQIjv9ZQpzDgrbpZUtpFi+Ogtj/DHaEubVd/pjCzlmxsD4zhR2Zcf8eb2rF3cZqWn8/MRDpByXRL5DTlfoClD4gTzwgCzRAeW72vL9rZmIAytYzlZlfiCR25SwstqvubNDgbjCAOpbps9o2jmofjKe+MjI7FjPkmDBV+wY2C6YTQWSg1dTJ8eA==";
    options.app_info = app_info;
//    edge_sdk::LoggerConsole console = {edge_sdk::kLevelDebug, PrintConsoleFunc, true};
//    options.logger_console_lists.push_back(console);
    options.key_store = std::make_shared<dji::KeyStoreDefault>();

}

DEFINE_int32(keep, 5, "日志清理周期 单位day，默认5");
DEFINE_bool(isSendSTDOUT, true, "输出到控制台，默认true");
DEFINE_string(logDir, "log", "日志的输出目录,默认log");

int main(int argc, char **argv) {

    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::string proFul = std::string(argv[0]);
    std::string pro = getFileName(proFul);
    //日志系统类
    GlogHelper glogHelper(pro, FLAGS_keep, FLAGS_logDir, FLAGS_isSendSTDOUT);

    edge_sdk::Options options;
    InitOptions(options);

    dji::DJI *dji = new dji::DJI(options);

    auto ret = dji->InitSDK();

    printf("InitSDK ret: %d\n", ret);

    auto liveview = std::make_shared<dji::DJILiveview>("myView");

    auto rc = liveview->Init(edge_sdk::Liveview::kCameraTypePayload, edge_sdk::Liveview::kStreamQuality540p);

    printf("liveview->Init ret: %d\n", rc);

    auto rc1 = liveview->Start();
    printf("liveview->Start ret: %d\n", rc1);
    auto rc2 = liveview->SetCameraSource(edge_sdk::Liveview::kCameraSourceWide);
    printf("liveview->SetCameraSource ret: %d\n", rc2);
    int count = 0;
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        count ++;
//        if (count == 5) {
//            break;
//        }
    }
    liveview->Stop();
    dji->DeInitSDK();
    delete dji;
    printf("exe exit\n");
    return 0;
}

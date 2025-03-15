//
// Created by lining on 2023/2/20.
//

#include "GlogHelper.h"
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <sys/stat.h>
#include <iostream>
#include "Poco/DirectoryIterator.h"
#include <Poco/Exception.h>

using Poco::DirectoryIterator;

GlogHelper::GlogHelper(std::string _program, int _keep, std::string _logDir, bool _isSendSTDOUT) :
        program(_program), keepDays(_keep), logDir(_logDir), isSendSTDOUT(_isSendSTDOUT) {

    FLAGS_log_dir = logDir;
    printf("日志目录:%s\n", logDir.c_str());
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 300;
    FLAGS_stop_logging_if_full_disk = true;
    if (isSendSTDOUT) {
        FLAGS_logtostderr = true;
    }
    google::InitGoogleLogging(program.data());
    google::InstallFailureSignalHandler();

    isRun = true;
    futureRun = std::async(std::launch::async, cleaner, this);
}

GlogHelper::~GlogHelper() {
    isRun = false;
    try {
        futureRun.wait();
    } catch (std::future_error &e) {
        LOG(ERROR) << e.what();
    }
    google::ShutdownGoogleLogging();
}

static void GetDirFiles(const std::string &path, std::vector<std::string> &array) {

    try {
        DirectoryIterator it(path);
        DirectoryIterator end;
        while (it != end) {
            if (it->isFile()) // 判断是文件还是子目录
            {
                array.push_back(it.name());
//                std::cout << it.path().toString() << std::endl;        //获取当前文件的的绝对路径名，it.name()只表示文件名
            }
//            else {
////                std::cout << "DirectoryName: " << it.path().toString() << std::endl;    //输出当前目录的绝对路径名（包含文件夹的名字）
//                GetDirFiles(it.path().toString(), array);
//            }
            ++it;
        }
    }
    catch (Poco::Exception &exc) {
//        std::cerr << exc.displayText() << std::endl;

    }
}

static bool startsWith(const std::string &str, const std::string &prefix) {
    return (str.rfind(prefix, 0) == 0);
}

static bool endsWith(const std::string &str, const std::string &suffix) {
    if (suffix.length() > str.length()) {
        return false;
    }

    return (str.rfind(suffix) == (str.length() - suffix.length()));
}

int GlogHelper::cleaner(GlogHelper *local) {
    if (local == nullptr) {
        return -1;
    }
    uint64_t keepSeconds = local->keepDays * 60 * 60 * 24;
    LOG(WARNING) << "start logfile cleaner";
    while (local->isRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(local->scanPeriod * 1000));
        std::vector<std::string> files;
        files.clear();
        GetDirFiles(local->logDir, files);
        std::vector<std::string> logs;
        logs.clear();
        for (auto &iter: files) {
            if (startsWith(iter, local->program + ".")) {
                logs.push_back(iter);
            }
        }
        if (!logs.empty()) {
            time_t now;
            time(&now);
            struct stat buf;
            for (auto &iter: logs) {
                std::string fulPath = local->logDir + "/" + iter;
                if (stat(fulPath.c_str(), &buf) == 0) {
                    if ((now - buf.st_ctime) > keepSeconds) {
                        LOG(INFO) << "准备删除文件 " << fulPath;
                        if (remove(fulPath.c_str()) == 0) {
                            char bufInfo[512];
                            memset(bufInfo, 0, 512);
                            sprintf(bufInfo, "log file:%s create time:%s,now:%s,keepSeconds:%ld s,delete",
                                    fulPath.c_str(), asctime(localtime(&buf.st_ctime)), asctime(localtime(&now)),
                                    keepSeconds);
                            LOG(INFO) << bufInfo;
                        }
                    }
                }
            }
        }
    }
    LOG(WARNING) << "关闭日志定时清理任务";
    return 0;
}

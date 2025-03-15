//
// Created by lining on 2023/2/20.
//

#ifndef GLOGHELPER_H
#define GLOGHELPER_H

#include <string>
#include <future>

class GlogHelper {
private:
    bool isRun = false;
    std::future<int> futureRun;

    int scanPeriod = 5;
    int keepDays = 1;
    std::string program;
    std::string logDir;
    bool isSendSTDOUT;
public:
    GlogHelper(std::string _program, int _keep, std::string _logDir, bool _isSendSTDOUT);

    ~GlogHelper();

private:
    static int cleaner(GlogHelper *local);

};


#endif //GLOGHELPER_H

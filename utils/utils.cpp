//
// Created by lining on 7/16/24.
//

#include "utils.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <iostream>
#include <sys/statfs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include "Poco/UUID.h"
#include "Poco/UUIDGenerator.h"

uint16_t swap_uint16(uint16_t val) {
    return ((val & 0xff00) >> 8) | ((val & 0x00ff) << 8);
}

uint32_t swap_uint32(uint32_t val) {
    return ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) |
           ((val & 0x000000ff) << 24);
}

vector<string> getDevList() {
    vector<string> list;
    // 打开 /dev 目录
    DIR *dir = opendir("/dev");
    if (dir == nullptr) {
        perror("opendir");
        return list;
    }

    // 读取目录项
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 过滤掉当前目录('.')和上级目录('..')
        if (entry->d_name[0] != '.') {
//            printf("%s\n", entry->d_name);
            list.emplace_back(entry->d_name);
        }
    }

    // 关闭目录
    closedir(dir);

    return list;
}


int runCmd(const std::string &command, std::string *output, bool redirect_stderr) {
    const auto &cmd = redirect_stderr ? command + " 2>&1" : command;
    auto pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        //记录日志
        return errno == 0 ? -1 : errno;
    }
    {
        char buffer[1024] = {0};
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (output) {
                output->append(buffer);
            }
//            printf("%s",buffer);
        }
    }
    return pclose(pipe);
}

uint64_t getTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

string getGuid() {
    Poco::UUID uuid = Poco::UUIDGenerator::defaultGenerator().createRandom();
    return uuid.toString();
}


// 取文件夹名字 无后缀
string getFolderPath(const string& str) {
    string::size_type idx = str.rfind('/', str.length());
    string folder = str.substr(0, idx);
    return folder;
}

// 取后缀
string getFileSuffix(const string& str) {
    string::size_type idx = str.rfind('.', str.length());
    string suffix = str.substr(idx + 1, str.length());
    return suffix;
}

// 取文件名字 不包括后缀
string getFileName(const string& str) {
    string::size_type idx = str.rfind('/', str.length());
    string::size_type pidx = str.rfind('.', str.length());
    string filename = str.substr(idx + 1, pidx - (idx + 1));
    return filename;
}

// 去掉后缀
string getRemoveSuffix(const string& str) {
    string::size_type idx = str.rfind('.', str.length());
    string filename = str.substr(0, idx);
    return filename;
}

// 取文件名字 包括后缀
string getFileNameAll(const string& str) {
    string::size_type idx = str.rfind('/', str.length());
    string name_all = str.substr(idx + 1, str.length());
    return name_all;
}

int GetVectorFromFile(vector<uint8_t> &array, const string& filePath) {
    ifstream fout;
    fout.open(filePath.c_str(), ios::in | ios::binary);
    if (fout.is_open()) {
        char val;
        while (fout.get(val)) {
            array.push_back(val);
        }
        fout.seekg(0, ios::beg);
        fout.close();
    } else {
        return -1;
    }

    return 0;
}

int GetFileFromVector(vector<uint8_t> &array, const string& filePath) {
    fstream fin;
    fin.open(filePath.c_str(), ios::out | ios::binary | ios::trunc);
    if (fin.is_open()) {
        for (auto iter: array) {
            fin.put(iter);
        }
        fin.flush();
        fin.close();
    } else {
        return -1;
    }

    return 0;
}

vector<string> StringSplit(const string &in, const string &delim) {
    stringstream tran(in.c_str());
    string tmp;
    vector<string> out;
    out.clear();
    while (std::getline(tran, tmp, *(delim.c_str()))) {
        out.push_back(tmp);
    }
    return out;
}

void PrintHex(uint8_t *data, uint32_t len) {
    int count = 0;
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        count++;
        if (count == 16) {
            printf("\n");
            count = 0;
        }
    }
    printf("\n");
}

wstring GbkToUnicode(const string& str) {
//    codecvt_byname<wchar_t, char, mbstate_t>*dd=;
    wstring_convert<chs_codecvt> gbk(new chs_codecvt("zh_CN.GBK"));    //GBK - whar

    return gbk.from_bytes(str);
}

string GbkToUtf8(const string& str) {
    return UnicodeToUtf8(GbkToUnicode(str));
}

string Utf8ToGbk(const string& str) {
    return UnicodeToGbk(Utf8ToUnicode(str));

}

wstring Utf8ToUnicode(const string& str) {
    wstring ret;

    wstring_convert<codecvt_utf8<wchar_t>> wcv;
    ret = wcv.from_bytes(str);
    return ret;
}

string UnicodeToUtf8(const wstring& wstr) {
    string ret;
    wstring_convert<codecvt_utf8<wchar_t>> wcv;
    ret = wcv.to_bytes(wstr);
    return ret;
}

string UnicodeToGbk(const wstring& wstr) {
    wstring_convert<chs_codecvt> gbk(new chs_codecvt("zh_CN.GBK"));    //GBK - whar
    string ret = gbk.to_bytes(wstr);
    return ret;

}

void Trim(string &str, char trim) {
    std::string::iterator end_pos = std::remove(str.begin(), str.end(), trim);
    str.erase(end_pos, str.end());
}

bool startsWith(const std::string str, const std::string prefix) {
    return (str.rfind(prefix, 0) == 0);
}

bool endsWith(const std::string str, const std::string suffix) {
    if (suffix.length() > str.length()) {
        return false;
    }

    return (str.rfind(suffix) == (str.length() - suffix.length()));
}

const char encodeCharacterTable[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const signed char decodeCharacterTable[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

void
base64_encode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length) {
    char buff1[3];
    char buff2[4];
    unsigned char i = 0, j;
    unsigned int input_cnt = 0;
    unsigned int output_cnt = 0;

    while (input_cnt < input_length) {
        buff1[i++] = input[input_cnt++];
        if (i == 3) {
            output[output_cnt++] = encodeCharacterTable[(buff1[0] & 0xfc) >> 2];
            output[output_cnt++] = encodeCharacterTable[((buff1[0] & 0x03) << 4) + ((buff1[1] & 0xf0) >> 4)];
            output[output_cnt++] = encodeCharacterTable[((buff1[1] & 0x0f) << 2) + ((buff1[2] & 0xc0) >> 6)];
            output[output_cnt++] = encodeCharacterTable[buff1[2] & 0x3f];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) {
            buff1[j] = '\0';
        }
        buff2[0] = (buff1[0] & 0xfc) >> 2;
        buff2[1] = ((buff1[0] & 0x03) << 4) + ((buff1[1] & 0xf0) >> 4);
        buff2[2] = ((buff1[1] & 0x0f) << 2) + ((buff1[2] & 0xc0) >> 6);
        buff2[3] = buff1[2] & 0x3f;
        for (j = 0; j < (i + 1); j++) {
            output[output_cnt++] = encodeCharacterTable[buff2[j]];
        }
        while (i++ < 3) {
            output[output_cnt++] = '=';
        }
    }
    output[output_cnt] = 0;
    *output_length = output_cnt;
}

void
base64_decode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length) {
    char buff1[4];
    char buff2[4];
    unsigned char i = 0, j;
    unsigned int input_cnt = 0;
    unsigned int output_cnt = 0;

    while (input_cnt < input_length) {
        buff2[i] = input[input_cnt++];
        if (buff2[i] == '=') {
            break;
        }
        if (++i == 4) {
            for (i = 0; i != 4; i++) {
                buff2[i] = decodeCharacterTable[buff2[i]];
            }
            output[output_cnt++] = (char) ((buff2[0] << 2) + ((buff2[1] & 0x30) >> 4));
            output[output_cnt++] = (char) (((buff2[1] & 0xf) << 4) + ((buff2[2] & 0x3c) >> 2));
            output[output_cnt++] = (char) (((buff2[2] & 0x3) << 6) + buff2[3]);
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) {
            buff2[j] = '\0';
        }
        for (j = 0; j < 4; j++) {
            buff2[j] = decodeCharacterTable[buff2[j]];
        }
        buff1[0] = (buff2[0] << 2) + ((buff2[1] & 0x30) >> 4);
        buff1[1] = ((buff2[1] & 0xf) << 4) + ((buff2[2] & 0x3c) >> 2);
        buff1[2] = ((buff2[2] & 0x3) << 6) + buff2[3];
        for (j = 0; j < (i - 1); j++) {
            output[output_cnt++] = (char) buff1[j];
        }
    }
//        output[output_cnt] = 0;
    *output_length = output_cnt;
}


string getIpStr(unsigned int ip) {
    union IPNode {
        unsigned int addr;
        struct {
            unsigned char s1;
            unsigned char s2;
            unsigned char s3;
            unsigned char s4;
        };
    };
    union IPNode x;

    x.addr = ip;
    char buffer[64];
    bzero(buffer, 64);
    sprintf(buffer, "%d.%d.%d.%d", x.s1, x.s2, x.s3, x.s4);

    return string(buffer);
}

bool isIPv4(string IP) {
    int dotcnt = 0;
    //数一共有几个.
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] == '.')
            dotcnt++;
    }
    //ipv4地址一定有3个点
    if (dotcnt != 3)
        return false;
    string temp = "";
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] != '.')
            temp += IP[i];
        //被.分割的每部分一定是数字0-255的数字
        if (IP[i] == '.' || i == IP.length() - 1) {
            if (temp.length() == 0 || temp.length() > 3)
                return false;
            for (int j = 0; j < temp.length(); j++) {
                if (!isdigit(temp[j]))
                    return false;
            }
            int tempInt = stoi(temp);
            if (tempInt > 255 || tempInt < 0)
                return false;
            string convertString = to_string(tempInt);
            if (convertString != temp)
                return false;
            temp = "";
        }
    }
    if (IP[IP.length() - 1] == '.')
        return false;
    return true;
}

bool isIPv6(string IP) {
    int dotcnt = 0;
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] == ':')
            dotcnt++;
    }
    if (dotcnt != 7) return false;
    string temp = "";
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] != ':')
            temp += IP[i];
        if (IP[i] == ':' || i == IP.length() - 1) {
            if (temp.length() == 0 || temp.length() > 4)
                return false;
            for (int j = 0; j < temp.length(); j++) {
                if (!(isdigit(temp[j]) || (temp[j] >= 'a' && temp[j] <= 'f') || (temp[j] >= 'A' && temp[j] <= 'F')))
                    return false;
            }
            temp = "";
        }
    }
    if (IP[IP.length() - 1] == ':')
        return false;
    return true;
}


string validIPAddress(string IP) {
    //以.和：来区分ipv4和ipv6
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] == '.')
            return isIPv4(IP) ? "IPv4" : "Neither";
        else if (IP[i] == ':')
            return isIPv6(IP) ? "IPv6" : "Neither";
    }
    return "Neither";
}

void GetDirFiles(const string& path, vector<string> &array) {
    DIR *dir;
    struct dirent *ptr;
    if ((dir = opendir(path.c_str())) == nullptr) {
        printf("can not open dir %s\n", path.c_str());
        return;
    } else {
        while ((ptr = readdir(dir)) != nullptr) {
            if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0)) {
//            printf("it is dir\n");
            } else if (ptr->d_type == DT_REG) {
                string name = ptr->d_name;
                array.push_back(name);
            }
        }
        closedir(dir);
    }
}

void CreatePath(const std::string& path) {

    DIR *dir = nullptr;
    dir = opendir(path.c_str());
    //目录不存在
    if (!dir) {
        vector<string> v = StringSplit(path, "/");
        string curpath;
        //一级一级的创建目录
        for (size_t i = 0; i < v.size(); i++) {
            curpath += v[i];
            curpath += "/";
            DIR *curdir = nullptr;
            curdir = opendir(curpath.c_str());
            if (!curdir) {
                mkdir(curpath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);//创建目录
            } else {
                closedir(curdir);
            }
        }
    } else {
        closedir(dir);
    }
}

double cpuUtilizationRatio() {
    string cmd = R"(top -bn1 |sed -n '3p' | awk -F 'ni,' '{print $2}' |cut -d. -f1 | sed 's/ //g')";
    std::string strRes;
    runCmd(cmd, &strRes);

    Trim(strRes, ' ');
    return 100.0 - atof(strRes.c_str());

}

double cpuTemperature() {
    FILE *fp = nullptr;
    int temp = 0;
    fp = fopen("/sys/devices/virtual/thermal/thermal_zone0/temp", "r");
    if (fp == nullptr) {
        std::cerr << "CpuTemperature fail" << std::endl;
        return 0;
    }

    fscanf(fp, "%d", &temp);
    fclose(fp);
    return (double) temp / 1000;
}

int memoryInfo(int &total, int &free) {
    int mem_free = -1;//空闲的内存，=总内存-使用了的内存
    int mem_total = -1; //当前系统可用总内存
    int mem_buffers = -1;//缓存区的内存大小
    int mem_cached = -1;//缓存区的内存大小
    char name[20];

    FILE *fp;
    char buf1[128], buf2[128], buf3[128], buf4[128], buf5[128];
    int buff_len = 128;
    fp = fopen("/proc/meminfo", "r");
    if (fp == nullptr) {
        std::cerr << "GetSysMemInfo() error! file not exist" << std::endl;
        return -1;
    }
    if (nullptr == fgets(buf1, buff_len, fp) ||
        nullptr == fgets(buf2, buff_len, fp) ||
        nullptr == fgets(buf3, buff_len, fp) ||
        nullptr == fgets(buf4, buff_len, fp) ||
        nullptr == fgets(buf5, buff_len, fp)) {
        std::cerr << "GetSysMemInfo() error! fail to read!" << std::endl;
        fclose(fp);
        return -1;
    }
    fclose(fp);
    sscanf(buf1, "%s%d", name, &mem_total);
    sscanf(buf2, "%s%d", name, &mem_free);
    sscanf(buf4, "%s%d", name, &mem_buffers);
    sscanf(buf5, "%s%d", name, &mem_cached);

    total = (double) mem_total / 1024.0;
    free = (double) mem_free / 1024.0;

    return 0;
}

int dirInfo(const string& dir, int &total, int &free) {
    struct statfs diskInfo;
    // 设备挂载的节点
    if (statfs(dir.c_str(), &diskInfo) == 0) {
        uint64_t blocksize = diskInfo.f_bsize;                   // 每一个block里包含的字节数
        uint64_t totalsize = blocksize * diskInfo.f_blocks;      // 总的字节数，f_blocks为block的数目
        uint64_t freeDisk = diskInfo.f_bfree * blocksize;       // 剩余空间的大小
        uint64_t availableDisk = diskInfo.f_bavail * blocksize; // 可用空间大小
        total = (double) totalsize / (1024.0 * 1024.0);
        free = (double) freeDisk / (1024.0 * 1024.0);
        return 0;
    } else {
        total = 0;
        free = 0;
        return -1;
    }
}

int getMAC(string &mac) {
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[2048];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        return -1;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
        close(sock);
        return -1;
    }

    struct ifreq *it = ifc.ifc_req;
    const struct ifreq *const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    char szMac[64];
    int count = 0;
    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    count++;
                    unsigned char *ptr;
                    ptr = (unsigned char *) &ifr.ifr_ifru.ifru_hwaddr.sa_data[0];
                    snprintf(szMac, 64, "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3),
                             *(ptr + 4), *(ptr + 5));
                    mac = string(szMac);
                    break;
                }
            }
        } else {
            close(sock);
            return -1;
        }
    }

    close(sock);
    return 0;
}

/*取板卡本地ip地址和n2n地址 */
int getIpaddr(string &ethIp, string &n2nIp) {
    int ret = 0;
    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *pifAddrStruct = nullptr;
    void *tmpAddrPtr;

    getifaddrs(&ifAddrStruct);

    if (ifAddrStruct != nullptr) {
        pifAddrStruct = ifAddrStruct;
    }

    while (ifAddrStruct != nullptr) {
        if (ifAddrStruct->ifa_addr == nullptr) {
            ifAddrStruct = ifAddrStruct->ifa_next;
            continue;
        }

        if (ifAddrStruct->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *) ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (strcmp(ifAddrStruct->ifa_name, "eth0") == 0) {
                ethIp = string(addressBuffer);
            } else if (strcmp(ifAddrStruct->ifa_name, "n2n0") == 0) {
                n2nIp = string(addressBuffer);
            }
        }

        ifAddrStruct = ifAddrStruct->ifa_next;
    }

    if (pifAddrStruct != nullptr) {
        freeifaddrs(pifAddrStruct);
    }

    return ret;
}

bool isProcessRun(string proc) {
    bool ret = false;
    string cmd = "ps -C " + proc + " |wc -l";
    string output;
    runCmd(cmd, &output);
    if (!output.empty()) {
        int num = atoi(output.c_str());
        if (num > 1) {
            ret = true;
        }
    }

    return ret;
}
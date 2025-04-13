//
// Created by lining on 7/16/24.
//

#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include <codecvt>
#include <locale>
#include "iconv.h"

using namespace std;

//大小端转换
uint16_t swap_uint16(uint16_t val);

uint32_t swap_uint32(uint32_t val);

//获取/dev目录下的文件集合
vector<string> getDevList();

int runCmd(const std::string &command, std::string *output = nullptr,
           bool redirect_stderr = false);

uint64_t getTimestampMs();

string getGuid();

// 取文件夹名字 无后缀
string getFolderPath(const string &str);

// 取后缀
string getFileSuffix(const string &str);

// 取文件名字 不包括后缀
string getFileName(const string &str);

// 去掉后缀
string getRemoveSuffix(const string &str);

// 取文件名字 包括后缀
string getFileNameAll(const string &str);

int GetVectorFromFile(vector<uint8_t> &array, const string &filePath);

int GetFileFromVector(vector<uint8_t> &array, const string &filePath);

vector<string> StringSplit(const string &in, const string &delim);

/**
 * 打印hex输出
 * @param data
 * @param len
 */
void PrintHex(uint8_t *data, uint32_t len);

class chs_codecvt : public codecvt_byname<wchar_t, char, std::mbstate_t> {
public:
    chs_codecvt(string s) : codecvt_byname(s) {

    }
};

string Utf8ToGbk(const string &str);

string GbkToUtf8(const string &str);

wstring Utf8ToUnicode(const string &str);

wstring GbkToUnicode(const string &str);

string UnicodeToUtf8(const wstring& str);

string UnicodeToGbk(const wstring& str);


void Trim(string &str, char trim);

bool startsWith(const std::string str, const std::string prefix);

bool endsWith(const std::string str, const std::string suffix);

void
base64_encode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length);

void
base64_decode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length);

string getIpStr(unsigned int ip);

bool isIPv4(string IP);

bool isIPv6(string IP);

string validIPAddress(string IP);

void GetDirFiles(const string& path, vector<string> &array);

//创建路径文件夹
void CreatePath(const std::string& path);

double cpuUtilizationRatio();

double cpuTemperature();

int memoryInfo(int &total, int &free);

int dirInfo(const string& dir, int &total, int &free);

int getMAC(string &mac);

int getIpaddr(string &ethIp, string &n2nIp);

bool isProcessRun(string proc);

#endif //UTILS_H

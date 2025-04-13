//
// Created by lining on 4/1/25.
//

#ifndef DJITHERMAL_H
#define DJITHERMAL_H

#include "dirp_api.h"
#include <string>

using namespace std;

namespace dji {
    class DJIThermal {
    public:
        typedef enum {
            dirp_action_type_extract = 0,
            dirp_action_type_measure,
            dirp_action_type_process,
            dirp_action_type_num,
        } dirp_action_type_e;

        typedef enum {
            dirp_measure_format_int16 = 0,
            dirp_measure_format_float32,
            dirp_measure_format_num,
        } dirp_measure_format_e;


        typedef struct MEASURE {
            float distance = 0.0;
            float humidity = 0.0;
            float emissivity = 0.0;
            float reflection = 0.0;
            float ambient = 0.0;
        } MEASURE;

        typedef struct ISOTherm {
            bool isEnable = false;
            float low = 25.0;
            float high = 30.0;
        } ISOTherm;

        typedef struct ColorBar {
            bool isManualEnable = false;
            float low = 25.0;
            float high = 30.0;
        } ColorBar;


        typedef struct ISPConfig {
            int32_t brightness = 0;//enhancement
            MEASURE measure;//measure
            string palette;//pseudo_color
            ISOTherm isotherm;//isotherm
            ColorBar colorBar;//color_bar
            //输出结果数组大小相关
            dirp_measure_format_e measurefmt = dirp_measure_format_int16;
            bool stretch = false;
        } ISPConfig;


    private:
        DIRP_HANDLE _handle = nullptr;
        string _rjpeg_in;
        string _rjpeg_out;


        int32_t _rjpeg_size = 0;
        uint8_t *_rjpeg_data = nullptr;
        dirp_resolution_t _rjpeg_resolution;
        //action
        dirp_action_type_e _action_type = dirp_action_type_num;
        dirp_isotherm_t _s_isotherm;
        dirp_color_bar_t _s_color_bar;
        ISPConfig _ispConfig;

    public:

        DJIThermal();

        ~DJIThermal();

    public:

        /**
         * 初始化handle
         * @param jpegPath RJPEG文件路径
         * @return
         */
        int Init(string jpegPath);

        /**
         * 获取TSDK版本
         * @return
         */
        string GetSDKVersion();

        /**
         * 获取RJPEG文件信息
         */
        void PrintRJPEGInfo();

        /**
         * 执行从原始JPEG文件到温度数组转换
         * @param out 输出结果图片的路径
         * @return
         */
        int32_t Process(dirp_action_type_e action_type, string out);

        /**
         * 释放占用资源
         * @return
         */
        int Release();

    private:
        int32_t prv_rjpeg_info_print(DIRP_HANDLE dirp_handle);

        //=========isp设置相关
        int32_t prv_isp_config(DIRP_HANDLE dirp_handle);

        int32_t prv_get_measurement_params(dirp_measurement_params_t *measurement_params, bool *modified);

        int32_t prv_get_enhancement_params(dirp_enhancement_params_t *enhancement_params, bool *modified);

        dirp_pseudo_color_e prv_get_pseudo_color(dirp_pseudo_color_e pseudo_color_old, bool *modified);

        int32_t prv_get_isotherm_params(dirp_isotherm_t *isotherm);

        int32_t prv_get_color_bar_params(dirp_color_bar_t *color_bar);

        //=======measurement 设置相关
        int32_t prv_measurement_config(DIRP_HANDLE dirp_handle);

        //获取输出RJPEG数组大小
        int32_t prv_get_rjpeg_output_size(const dirp_action_type_e action_type, const dirp_resolution_t *resolution);
    };
}


#endif //DJITHERMAL_H

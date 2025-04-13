//
// Created by lining on 4/1/25.
//

#include "DJIThermal.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>

#define GLOG_NO_ABBREVIATED_SEVERITIES

#include "glog/logging.h"

namespace dji {
    DJIThermal::DJIThermal() {

    }

    DJIThermal::~DJIThermal() {

    }


    int DJIThermal::Init(string jpegPath) {
        //判断照片是否存在
        if (access(jpegPath.c_str(), F_OK) != 0) {
            LOG(ERROR) << "jpeg file not exist:" << jpegPath;
            return -1;
        }
        _rjpeg_in = jpegPath;
        //获取照片文件的属性
        struct stat rjpeg_file_info;
        stat(jpegPath.c_str(), &rjpeg_file_info);
        _rjpeg_size = rjpeg_file_info.st_size;
        _rjpeg_data = new uint8_t[_rjpeg_size];
        if (_rjpeg_data == nullptr) {
            LOG(ERROR) << "_rjpeg_data nullptr";
            return -1;
        }
        //打开文件并读取到内存中
        ifstream fs_jpeg(jpegPath, ios::binary);
        if (!fs_jpeg.is_open()) {
            LOG(ERROR) << "open jpeg file failed";
            return -1;
        }
        fs_jpeg.read((char *) _rjpeg_data, _rjpeg_size);
        fs_jpeg.close();
        //创建handle
        int ret = dirp_create_from_rjpeg(_rjpeg_data, _rjpeg_size, &_handle);
        if (ret != DIRP_SUCCESS) {
            LOG(ERROR) << "create R-JPEG dirp handle failed";
            return -1;
        }
        return 0;
    }

    string DJIThermal::GetSDKVersion() {
        dirp_api_version_t version;
        int ret = dirp_get_api_version(_handle, &version);
        if (ret != DIRP_SUCCESS) {
            LOG(ERROR) << "get dirp api version failed";
            return std::string();
        }
//        std::ostringstream oss;
//        oss << std::hex << version.api;
//        return oss.str() + "." + std::string(version.magic);
        return to_string(version.api) + "." + std::string(version.magic);
    }

    void DJIThermal::PrintRJPEGInfo() {
        int ret = dirp_get_infared_density_filter_installed(_handle);
        if (ret == DIRP_SUCCESS) {
            LOG(INFO) << "Infared density filter installed";
            ret = prv_rjpeg_info_print(_handle);
        } else {
            LOG(INFO) << "Infared density filter not installed";
        }
    }

    int32_t DJIThermal::Process(dirp_action_type_e action_type, string out) {
        _rjpeg_out = out;
        int32_t ret = DIRP_SUCCESS;
        int out_size = 0;
        dirp_resolution_t rjpeg_resolution = {0, 0};
        void *raw_out = nullptr;
        dirp_measure_format_e measure_format = _ispConfig.measurefmt;
        //1.新建输出文件
        ofstream ofstream;
        ofstream.open(_rjpeg_out.c_str(), ios::binary);
        if (!ofstream.is_open()) {
            cout << "ERROR: create ofstream failed" << endl;
            ret = -1;
            goto ERR_ACT_RET;
        }
        //2.获取rjpeg分辨率
        ret = dirp_get_rjpeg_resolution(_handle, &rjpeg_resolution);
        if (ret != DIRP_SUCCESS) {
            LOG(ERROR) << "get rjpeg resolution failed";
            goto ERR_ACT_RET;
        }
        //3.获取输出内存的大小
        _action_type = action_type;
        out_size = prv_get_rjpeg_output_size(_action_type, &rjpeg_resolution);
        if (out_size <= 0) {
            LOG(ERROR) << "get rjpeg output size failed";
            goto ERR_ACT_RET;
        }

        //4.申请输出空间内存
        raw_out = malloc(out_size);
        if (raw_out == nullptr) {
            LOG(ERROR) << "raw out malloc failed";
            goto ERR_ACT_RET;
        }

        //4.按动作执行相应动作,有sdk设置，也有获取输出
        //4.1设置动作
        if (action_type == dirp_action_type_process) {
            ret = prv_isp_config(_handle);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call prv_isp_config failed" << endl;
                goto ERR_ACT_RET;
            }
        }

        if ((action_type == dirp_action_type_measure) || (action_type == dirp_action_type_process)) {
            ret = prv_measurement_config(_handle);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call prv_isp_config failed" << endl;
                goto ERR_ACT_RET;
            }
        }

        //4.2获取输出
        switch (action_type) {
            case dirp_action_type_extract: {
                ret = dirp_get_original_raw(_handle, (uint16_t *) raw_out, out_size);
            }
                break;
            case dirp_action_type_measure: {
                if (measure_format == dirp_measure_format_float32) {
                    ret = dirp_measure_ex(_handle, (float *) raw_out, out_size);
                } else {
                    ret = dirp_measure(_handle, (int16_t *) raw_out, out_size);
                }
            }
                break;
            case dirp_action_type_process: {
                if (_ispConfig.stretch) {
                    ret = dirp_process_strech(_handle, (float *) raw_out, out_size);
                } else {
                    ret = dirp_process(_handle, (uint8_t *) raw_out, out_size);
                }
            }
                break;
        }

        if (ret != DIRP_SUCCESS) {
            LOG(ERROR) << "call dirp_get_[original_raw/measure/process] failed";
            goto ERR_ACT_RET;
        }

        //5.将内存的结果写入文件
        ofstream.write((const char *) raw_out, out_size);
        LOG(INFO) << "write " << out_size << " bytes to file";

        //6.获取下color_bar
        if (action_type == dirp_action_type_process) {
            if (!_s_color_bar.manual_enable) {
                dirp_color_bar_t color_bar_adaptive = {0};
                ret = dirp_get_color_bar_adaptive_params(_handle, &color_bar_adaptive);
                if (ret == DIRP_SUCCESS) {
                    LOG(INFO) << "Color bar adaptive range is [" << color_bar_adaptive.low << ","
                              << color_bar_adaptive.high << "]";
                }
            }
        }

        ERR_ACT_RET:
        if (ofstream.is_open())
            ofstream.close();

        if (raw_out)
            free(raw_out);

        return ret;
    }

    int DJIThermal::Release() {
        if (_handle != nullptr) {
            int status = dirp_destroy(_handle);
            if (status != DIRP_SUCCESS) {
                LOG(ERROR) << "destroy dirp handle failed";
            }
        }

        if (_rjpeg_data != nullptr) {
            delete[] _rjpeg_data;
            _rjpeg_data = nullptr;
        }
    }

    int32_t DJIThermal::prv_rjpeg_info_print(DIRP_HANDLE dirp_handle) {
        int32_t ret = DIRP_SUCCESS;
        dirp_rjpeg_version_t rjpeg_version = {0};
        dirp_resolution_t rjpeg_resolution = {0};

        ret = dirp_get_rjpeg_version(dirp_handle, &rjpeg_version);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_rjpeg_version failed" << endl;
            goto ERR_RJPEG_INFO_PRINT_RET;
        }
        cout << "R-JPEG version information" << endl;
        cout << "    R-JPEG version : 0x" << hex << rjpeg_version.rjpeg << dec << endl;
        cout << "    header version : 0x" << hex << rjpeg_version.header << dec << endl;
        cout << " curve LUT version : 0x" << hex << rjpeg_version.curve << dec << endl;

        ret = dirp_get_rjpeg_resolution(dirp_handle, &rjpeg_resolution);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_rjpeg_version failed" << endl;
            goto ERR_RJPEG_INFO_PRINT_RET;
        }
        _rjpeg_resolution = rjpeg_resolution;
        cout << "R-JPEG resolution size" << endl;
        cout << "      image  width : " << rjpeg_resolution.width << endl;
        cout << "      image height : " << rjpeg_resolution.height << endl;

        ERR_RJPEG_INFO_PRINT_RET:
        return ret;
    }

    int32_t DJIThermal::prv_isp_config(DIRP_HANDLE dirp_handle) {
        int32_t ret = DIRP_SUCCESS;
        bool modified = false;
        dirp_enhancement_params_t enhancement_params = {0};
        dirp_pseudo_color_e pseudo_color_old = DIRP_PSEUDO_COLOR_IRONRED;
        dirp_pseudo_color_e pseudo_color_new = DIRP_PSEUDO_COLOR_IRONRED;

        dirp_measurement_params_t measurement_params = {0};
        dirp_measurement_params_range_t params_range = {0};

        /* Get original measurement parameters */
        ret = dirp_get_measurement_params(dirp_handle, &measurement_params);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_measurement_params failed" << endl;
            goto ERR_ISP_CONFIG_RET;
        }

        /* Refresh custom measurement parameters */
        ret = prv_get_measurement_params(&measurement_params, &modified);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call argparse_get_measurement_params failed" << endl;
            goto ERR_ISP_CONFIG_RET;
        }

        ret = dirp_get_measurement_params_range(dirp_handle, &params_range);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_measurement_params_range failed" << endl;
            goto ERR_ISP_CONFIG_RET;
        }
        cout << "Measurement: get params range:" << endl;
        cout << "distance: [" << params_range.distance.min << "," << params_range.distance.max << "]" << endl;
        cout << "humidity: [" << params_range.humidity.min << "," << params_range.humidity.max << "]" << endl;
        cout << "emissivity: [" << params_range.emissivity.min << "," << params_range.emissivity.max << "]" << endl;
        cout << "ambient_temp: [" << params_range.ambient_temp.min << "," << params_range.ambient_temp.max << "]"
             << endl;
        cout << "reflection: [" << params_range.reflection.min << "," << params_range.reflection.max << "]" << endl;

        /* Set pseudo color type */
        ret = dirp_get_pseudo_color(dirp_handle, &pseudo_color_old);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_pseudo_color failed" << endl;
            goto ERR_ISP_CONFIG_RET;
        }

        /* Refresh custom pseudo color type */
        modified = false;
        pseudo_color_new = prv_get_pseudo_color(pseudo_color_old, &modified);
        if (modified) {
            ret = dirp_set_pseudo_color(dirp_handle, pseudo_color_new);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call dirp_set_pseudo_color failed" << endl;
                goto ERR_ISP_CONFIG_RET;
            }
        }

        /* Set isotherm parameters */
        ret = prv_get_isotherm_params(&_s_isotherm);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call argparse_get_isotherm_params failed" << endl;
            _s_isotherm.enable = false;
        }
        if (_s_isotherm.enable) {
            ret = dirp_set_isotherm(dirp_handle, &_s_isotherm);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call dirp_set_isotherm failed" << endl;
                goto ERR_ISP_CONFIG_RET;
            }
        }

        /* Set color bar parameters */
        ret = prv_get_color_bar_params(&_s_color_bar);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call argparse_get_color_bar_params failed" << endl;
            _s_color_bar.manual_enable = false;
        }
        if (_s_color_bar.manual_enable) {
            ret = dirp_set_color_bar(dirp_handle, &_s_color_bar);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call dirp_set_color_bar failed" << endl;
                goto ERR_ISP_CONFIG_RET;
            }
        }

        /* Get original enhancement parameters */
        ret = dirp_get_enhancement_params(dirp_handle, &enhancement_params);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_enhancement_params failed" << endl;
            goto ERR_ISP_CONFIG_RET;
        }

        /* Refresh custom enhancement parameters */
        modified = false;
        ret = prv_get_enhancement_params(&enhancement_params, &modified);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call argparse_get_enhancement_params failed" << endl;
            goto ERR_ISP_CONFIG_RET;
        }
        if (modified) {
            /* Set custom enhancement parameters */
            ret = dirp_set_enhancement_params(dirp_handle, &enhancement_params);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call dirp_set_enhancement_params failed" << endl;
                goto ERR_ISP_CONFIG_RET;
            }
        }

        /* TODO: Set another ISP processing parameters */

        ERR_ISP_CONFIG_RET:
        return ret;
    }

    int32_t DJIThermal::prv_get_measurement_params(dirp_measurement_params_t *measurement_params, bool *modified) {
        int32_t ret = DIRP_SUCCESS;
        *modified = false;

        if (_ispConfig.measure.distance > 0) {
            *modified = true;
            float original_distance = measurement_params->distance;
            measurement_params->distance = _ispConfig.measure.distance;
            cout << "Change distance from " << original_distance << " to " << measurement_params->distance << endl;
        }
        if (_ispConfig.measure.humidity > 0) {
            *modified = true;
            float original_humidity = measurement_params->humidity;
            measurement_params->humidity = _ispConfig.measure.humidity;
            cout << "Change humidity from " << original_humidity << " to " << measurement_params->humidity << endl;
        }
        if (_ispConfig.measure.emissivity > 0) {
            *modified = true;
            float original_emissivity = measurement_params->emissivity;
            measurement_params->emissivity = _ispConfig.measure.emissivity;
            cout << "Change emissivity from " << original_emissivity << " to " << measurement_params->emissivity
                 << endl;
        }
        if (_ispConfig.measure.reflection > 0) {
            *modified = true;
            float original_reflection = measurement_params->reflection;
            measurement_params->reflection = _ispConfig.measure.reflection;
            cout << "Change reflection from " << original_reflection << " to " << measurement_params->reflection
                 << endl;
        }
        if (_ispConfig.measure.ambient > 0) {
            *modified = true;
            float original_ambient_temp = measurement_params->ambient_temp;
            measurement_params->ambient_temp = _ispConfig.measure.ambient;
            cout << "Change ambient_temp from " << original_ambient_temp << " to " << measurement_params->ambient_temp
                 << endl;
        }

        return ret;
    }

    int32_t DJIThermal::prv_get_enhancement_params(dirp_enhancement_params_t *enhancement_params, bool *modified) {
        int32_t ret = DIRP_SUCCESS;
        *modified = false;

        if (_ispConfig.brightness >= 0) {
            *modified = true;
            int32_t original_brightness = enhancement_params->brightness;
            enhancement_params->brightness = _ispConfig.brightness;
            cout << "Change brightness from " << original_brightness << " to " << enhancement_params->brightness
                 << endl;
        }

        return ret;
    }

    dirp_pseudo_color_e DJIThermal::prv_get_pseudo_color(dirp_pseudo_color_e pseudo_color_old, bool *modified) {
        dirp_pseudo_color_e pseudo_color_new = pseudo_color_old;
        static map<string, dirp_pseudo_color_e> pseudo_color_tables = {
                {"white_hot", DIRP_PSEUDO_COLOR_WHITEHOT},
                {"fulgurite", DIRP_PSEUDO_COLOR_FULGURITE},
                {"iron_red",  DIRP_PSEUDO_COLOR_IRONRED},
                {"hot_iron",  DIRP_PSEUDO_COLOR_HOTIRON},
                {"medical",   DIRP_PSEUDO_COLOR_MEDICAL},
                {"arctic",    DIRP_PSEUDO_COLOR_ARCTIC},
                {"rainbow1",  DIRP_PSEUDO_COLOR_RAINBOW1},
                {"rainbow2",  DIRP_PSEUDO_COLOR_RAINBOW2},
                {"tint",      DIRP_PSEUDO_COLOR_TINT},
                {"black_hot", DIRP_PSEUDO_COLOR_BLACKHOT},
        };


        *modified = false;
        if (!_ispConfig.palette.empty()) {
            string pseudo_color_name = _ispConfig.palette;
            *modified = true;
            auto iter = pseudo_color_tables.find(pseudo_color_name);
            if (iter != pseudo_color_tables.end()) {
                pseudo_color_new = iter->second;
            } else {
                pseudo_color_new = DIRP_PSEUDO_COLOR_IRONRED;
            }
            cout << "Change pseudo color from " << pseudo_color_old << " to " << pseudo_color_new << endl;
        }

        return pseudo_color_new;
    }

    int32_t DJIThermal::prv_get_isotherm_params(dirp_isotherm_t *isotherm) {
        int32_t ret = DIRP_SUCCESS;
        isotherm->enable = _ispConfig.isotherm.isEnable;
        isotherm->low = _ispConfig.isotherm.low;
        isotherm->high = _ispConfig.isotherm.high;

        return ret;
    }

    int32_t DJIThermal::prv_get_color_bar_params(dirp_color_bar_t *color_bar) {
        int32_t ret = DIRP_SUCCESS;

        color_bar->manual_enable = _ispConfig.colorBar.isManualEnable;
        color_bar->low = _ispConfig.colorBar.low;
        color_bar->high = _ispConfig.colorBar.high;

        return ret;
    }

    int32_t DJIThermal::prv_measurement_config(DIRP_HANDLE dirp_handle) {
        int32_t ret = DIRP_SUCCESS;
        bool modified = false;
        dirp_measurement_params_t measurement_params = {0};
        dirp_measurement_params_range_t params_range = {0};

        /* Get original measurement parameters */
        ret = dirp_get_measurement_params(dirp_handle, &measurement_params);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_measurement_params failed" << endl;
            goto ERR_MEASUREMENT_CONFIG_RET;
        }

        /* Refresh custom measurement parameters */
        ret = prv_get_measurement_params(&measurement_params, &modified);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call argparse_get_measurement_params failed" << endl;
            goto ERR_MEASUREMENT_CONFIG_RET;
        }

        ret = dirp_get_measurement_params_range(dirp_handle, &params_range);
        if (DIRP_SUCCESS != ret) {
            cout << "ERROR: call dirp_get_measurement_params_range failed" << endl;
            goto ERR_MEASUREMENT_CONFIG_RET;
        }
        cout << "Measurement: get params range:" << endl;
        cout << "distance: [" << params_range.distance.min << "," << params_range.distance.max << "]" << endl;
        cout << "humidity: [" << params_range.humidity.min << "," << params_range.humidity.max << "]" << endl;
        cout << "emissivity: [" << params_range.emissivity.min << "," << params_range.emissivity.max << "]" << endl;
        cout << "ambientTemp: [" << params_range.ambient_temp.min << "," << params_range.ambient_temp.max << "]"
             << endl;
        cout << "reflection: [" << params_range.reflection.min << "," << params_range.reflection.max << "]" << endl;

        if (modified) {
            /* Set custom measurement parameters */
            ret = dirp_set_measurement_params(dirp_handle, &measurement_params);
            if (DIRP_SUCCESS != ret) {
                cout << "ERROR: call dirp_set_measurement_params failed" << endl;
                goto ERR_MEASUREMENT_CONFIG_RET;
            }
        }

        ERR_MEASUREMENT_CONFIG_RET:
        return ret;
    }

    int32_t DJIThermal::prv_get_rjpeg_output_size(const DJIThermal::dirp_action_type_e action_type,
                                                  const dirp_resolution_t *resolution) {
        int32_t image_width = resolution->width;
        int32_t image_height = resolution->height;
        int32_t image_size = 0;

        dirp_measure_format_e measure_format = _ispConfig.measurefmt;
        bool stretch_only = _ispConfig.stretch;

        switch (action_type) {
            case dirp_action_type_extract:
                image_size = image_width * image_height * sizeof(uint16_t);
                break;
            case dirp_action_type_measure:
                if (dirp_measure_format_float32 == measure_format) {
                    image_size = image_width * image_height * sizeof(float);
                } else {
                    image_size = image_width * image_height * sizeof(int16_t);
                }
                break;
            case dirp_action_type_process:
                if (stretch_only) {
                    image_size = image_width * image_height * sizeof(float);
                } else {
                    image_size = image_width * image_height * 3 * sizeof(uint8_t);
                }
                break;
        }

        return image_size;
    }


}


//
// Created by lining on 4/1/25.
//

#include "DJI/DJIThermal.h"
#include <memory>


int main() {

    auto djiThermal = make_shared<dji::DJIThermal>();
    if (djiThermal->Init("./thermal_dataset/DJI_0001_R.JPG") != 0) {
        return -1;
    }

    string version = djiThermal->GetSDKVersion();

    printf("version: %s\n", version.c_str());

    djiThermal->PrintRJPEGInfo();

    djiThermal->Process(dji::DJIThermal::dirp_action_type_measure,"./thermal_dataset/DJI_0001_R.raw");

    djiThermal->Release();


    return 0;
}
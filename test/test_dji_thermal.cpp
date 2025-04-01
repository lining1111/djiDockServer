//
// Created by lining on 4/1/25.
//

#include "DJI/DJIThermal.h"
#include <memory>


int main() {

    auto djiThermal = make_shared<dji::DJIThermal>();
    if (djiThermal->Init("./thermal_dataset/DJI_0002_R.JPG") != 0) {
        return -1;
    }

    string version = djiThermal->GetSDKVersion();

    printf("version: %s\n", version.c_str());

    djiThermal->PrintRJPEGInfo();



    return 0;
}
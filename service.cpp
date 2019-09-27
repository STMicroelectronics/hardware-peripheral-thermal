/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "android.hardware.thermal@1.1-service.stm32mp1"

#include <android-base/logging.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>

#include "Thermal.h"

using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::thermal::V1_1::IThermal;
using ::android::hardware::thermal::V1_1::implementation::Thermal;
using ::android::hardware::joinRpcThreadpool;
using ::android::OK;
using ::android::sp;

int main(int /* argc */, char* /* argv */ []) {

    sp<IThermal> thermal = new Thermal();
    if (thermal == nullptr) {
        LOG(ERROR) << "Can not create an instance of Thermal hardware Iface, exiting.";
        goto shutdown;
    }

    configureRpcThreadpool(1, true /* will join */);
    if (thermal->registerAsService() != OK) {
        LOG(ERROR) << "Could not register Thermal service.";
        goto shutdown;
    }
    joinRpcThreadpool();

shutdown:
    LOG(ERROR) << "Thermal service is shutting down";
    return 1;
}

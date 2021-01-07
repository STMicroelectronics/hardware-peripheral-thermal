/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <cerrno>
#include <set>
#include <vector>

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "Thermal.h"
#include "thermal-helper.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::sp;
using ::android::hardware::interfacesEqual;
using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;

std::set<sp<IThermalChangedCallback>> gCallbacks;

Thermal::Thermal() : enabled_(initThermal()) {}

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.

Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;

    std::vector<Temperature_1_0> temperatures;
    temperatures.resize(kTemperatureNum);

    if (!enabled_) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, temperatures);
        return Void();
    }

    ssize_t ret = fillTemperatures_1_0(&temperatures);
    if (ret == 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "No available sensor";
    }
    temperatures.resize(ret);

    _hidl_cb(status, temperatures);
    return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;

    std::vector<CpuUsage> cpuUsages;
    cpuUsages.resize(kCpuNum);

    if (!enabled_) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, cpuUsages);
        return Void();
    }

    ssize_t ret = fillCpuUsages(&cpuUsages);
    if (ret < 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(-ret);
    }

    _hidl_cb(status, cpuUsages);
    return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;

    std::vector<CoolingDevice_1_0> coolingDevices;
    coolingDevices.resize(1);

    if (!enabled_) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, coolingDevices);
        return Void();
    }

    ssize_t ret = fillCoolingDevices_1_0(&coolingDevices);
    if (ret == 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "No available cooling device";
    }
    coolingDevices.resize(ret);

    _hidl_cb(status, coolingDevices);
    return Void();
}

// Methods from ::android::hardware::thermal::V2_0::IThermal follow.

Return<void> Thermal::getCurrentTemperatures(bool filterType, TemperatureType type,
                                             getCurrentTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;

    std::vector<Temperature_2_0> temperatures;
    temperatures.resize(kTemperatureNum);

    if (!enabled_) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, temperatures);
        return Void();
    }

    ssize_t ret;
    if (filterType) {
        ret = fillTemperature_2_0(&temperatures, type);
    } else {
        ret = fillTemperatures_2_0(&temperatures);
    }
    if (ret == 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "No available sensor";
    }
    temperatures.resize(ret);

    _hidl_cb(status, temperatures);
    return Void();
}

Return<void> Thermal::getTemperatureThresholds(bool filterType, TemperatureType type,
                                               getTemperatureThresholds_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;

    std::vector<TemperatureThreshold> temperatureThresholds;
    temperatureThresholds.resize(kTemperatureNum);

    if (!enabled_) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, temperatureThresholds);
        return Void();
    }

    ssize_t ret;
    if (filterType) {
        ret = fillTemperatureThreshold(&temperatureThresholds, type);
    } else {
        ret = fillTemperaturesThreshold(&temperatureThresholds);
    }
    if (ret == 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "No available sensor";
    }
    temperatureThresholds.resize(ret);

    _hidl_cb(status, temperatureThresholds);
    return Void();
}

Return<void> Thermal::getCurrentCoolingDevices(bool filterType, CoolingType_2_0 type,
                                               getCurrentCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;

    std::vector<CoolingDevice_2_0> coolingDevices;
    coolingDevices.resize(kCoolingNum_2_0);

    if (!enabled_) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, coolingDevices);
        return Void();
    }

    ssize_t ret;
    if (filterType) {
        ret = fillCoolingDevice_2_0(&coolingDevices, type);
    } else {
        ret = fillCoolingDevices_2_0(&coolingDevices);
    }
    if (ret == 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "No available cooling device";
    }
    coolingDevices.resize(ret);

    _hidl_cb(status, coolingDevices);
    return Void();
}

Return<void> Thermal::registerThermalChangedCallback(const sp<IThermalChangedCallback>& callback,
                                                     bool filterType, TemperatureType type,
                                                     registerThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;
    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        LOG(ERROR) << status.debugMessage;
        _hidl_cb(status);
        return Void();
    } else {
        status.code = ThermalStatusCode::SUCCESS;
    }
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    if (std::any_of(callbacks_.begin(), callbacks_.end(), [&](const CallbackSetting& c) {
            return interfacesEqual(c.callback, callback);
        })) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Same callback interface registered already";
        LOG(ERROR) << status.debugMessage;
    } else {
        callbacks_.emplace_back(callback, filterType, type);
        LOG(INFO) << "A callback has been registered to ThermalHAL, isFilter: " << filterType
                  << " Type: " << android::hardware::thermal::V2_0::toString(type);
    }
    _hidl_cb(status);
    return Void();
}

Return<void> Thermal::unregisterThermalChangedCallback(
    const sp<IThermalChangedCallback>& callback, unregisterThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;
    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        LOG(ERROR) << status.debugMessage;
        _hidl_cb(status);
        return Void();
    } else {
        status.code = ThermalStatusCode::SUCCESS;
    }
    bool removed = false;
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    callbacks_.erase(
        std::remove_if(callbacks_.begin(), callbacks_.end(),
                       [&](const CallbackSetting& c) {
                           if (interfacesEqual(c.callback, callback)) {
                               LOG(INFO)
                                   << "A callback has been unregistered from ThermalHAL, isFilter: "
                                   << c.is_filter_type << " Type: "
                                   << android::hardware::thermal::V2_0::toString(c.type);
                               removed = true;
                               return true;
                           }
                           return false;
                       }),
        callbacks_.end());
    if (!removed) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "The callback was not registered before";
        LOG(ERROR) << status.debugMessage;
    }
    _hidl_cb(status);
    return Void();
}

// Local functions to be used internally by a thermal daemon

void Thermal::notifyThrottling(const Temperature& temperature) {

    std::vector<CallbackSetting>::const_iterator iterator;
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);

    if (callbacks_.size() > 0) {
        for (iterator=callbacks_.begin(); iterator!=callbacks_.end(); iterator++) {
            if ((*iterator).type == temperature.type || ! (*iterator).is_filter_type) {
                Return<void> ret = (*iterator).callback->notifyThrottling(temperature);
                if (!ret.isOk()) {
                    if (ret.isDeadObject()) {
                        LOG(WARNING) << "Dropped throttling event, ThermalChangedCallback died";
                    } else {
                        LOG(WARNING) << "Failed to send throttling event to ThermalChangedCallback";
                    }
                }
            }
        }
    }

}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

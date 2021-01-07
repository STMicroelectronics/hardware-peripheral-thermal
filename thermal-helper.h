/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __THERMAL_HELPER_H__
#define __THERMAL_HELPER_H__

#include <android/hardware/thermal/2.0/IThermal.h>

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::thermal::V1_0::CpuUsage;
using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingType_1_0 = ::android::hardware::thermal::V1_0::CoolingType;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;

using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using CoolingType_2_0 = ::android::hardware::thermal::V2_0::CoolingType;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;



// Maximum number of sensors treated
constexpr unsigned int kCpuNum = 2;
constexpr unsigned int kTemperatureNum = 3 + kCpuNum;

// Maximum number of cooling devices treated
constexpr unsigned int kCoolingNum_2_0 = 2;

// Path to get back CPU usage data
constexpr const char *kCpuUsageFile = "/proc/stat";
constexpr const char *kCpuOnlineFileFormat = "/sys/devices/system/cpu/cpu%d/online";

// Path to get back thermal zone data
constexpr const char *kThermalZoneTypeFileFormat = "/sys/class/thermal/thermal_zone%d/type";
constexpr const char *kThermalZoneTempFileFormat = "/sys/class/thermal/thermal_zone%d/temp";

// Path to get back trip point information
constexpr const char *kTripTypeFileFormat = "/sys/class/thermal/thermal_zone%d/trip_point_%d_type";
constexpr const char *kTripTempFileFormat = "/sys/class/thermal/thermal_zone%d/trip_point_%d_temp";

// Path to get back cooling device data
constexpr const char *kCoolingDeviceTypeFileFormat = "/sys/class/thermal/cooling_device%d/type";
constexpr const char *kCoolingDeviceCurStateFileFormat = "/sys/class/thermal/cooling_device%d/cur_state";
constexpr const char *kCoolingDeviceMaxStateFileFormat = "/sys/class/thermal/cooling_device%d/max_state";

// Used to get information on scanned thermal zone
constexpr unsigned int kMaxThermalZones = 3;
constexpr unsigned int kMaxThermalTrip = 3;

struct thermal_trip_t {
    int             nb_trip;
    char            trip_type[kMaxThermalTrip][32];
};

struct thermal_zone_t {
    int             nb_zone;
    char            zone_type[kMaxThermalZones][32];
    thermal_trip_t  trip[kMaxThermalZones];
};

// Used to get information on scanned cooling device
constexpr unsigned int kMaxCoolingDevices = 3;

struct cooling_device_t {
    int             nb_cooling;
    char            cooling_type[kMaxCoolingDevices][32];
};

bool initThermal();

ssize_t fillTemperatures_1_0(std::vector<Temperature_1_0> *temperatures);
ssize_t fillCoolingDevices_1_0(std::vector<CoolingDevice_1_0> *cooling);

ssize_t fillTemperatures_2_0(std::vector<Temperature_2_0> *temperatures);
ssize_t fillTemperature_2_0(std::vector<Temperature_2_0> *temperatures, TemperatureType type);

ssize_t fillTemperaturesThreshold(std::vector<TemperatureThreshold> *temperature_thresholds);
ssize_t fillTemperatureThreshold(std::vector<TemperatureThreshold> *temperature_thresholds, TemperatureType type);

ssize_t fillCoolingDevices_2_0(std::vector<CoolingDevice_2_0> *cooling);
ssize_t fillCoolingDevice_2_0(std::vector<CoolingDevice_2_0> *cooling, CoolingType_2_0 type);

ssize_t fillCpuUsages(std::vector<CpuUsage> *cpuUsages);

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif //__THERMAL_HELPER_H__

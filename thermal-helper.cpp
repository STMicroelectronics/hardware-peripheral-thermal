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

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>

#include "thermal-helper.h"

extern "C" {
#include "thermal-config.h"
}

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

using ::android::hardware::thermal::V1_0::TemperatureType;

static thermal_config_t gThermalConfig[THERMAL_CONFIG_MAX];

/**
 * Initialization constants based on platform
 *
 * @return true on success or false on error.
 */
bool initThermal() {
    memset(&gThermalConfig,0,sizeof(thermal_config_t)*THERMAL_CONFIG_MAX);
    int ret = parse_config_file(gThermalConfig);
    if (ret != 0) {
        LOG(ERROR) << "failed to parse thermal configuration file";
        return false;
    }
    return true;
}

/**
 * Reads device temperature.
 *
 * @param sensor_num Number of sensor file with temperature.
 * @param type Device temperature type.
 * @param name Device temperature name.
 * @param mult Multiplier used to translate temperature to Celsius.
 * @param throttling_threshold Throttling threshold for the temperature.
 * @param shutdown_threshold Shutdown threshold for the temperature.
 * @param out Pointer to temperature_t structure that will be filled with current
 *     values.
 *
 * @return 0 on success or negative value -errno on error.
 */
static ssize_t readTemperature(int sensor_num, TemperatureType type, const char *name, float mult,
                                float throttling_threshold, float shutdown_threshold,
                                float vr_throttling_threshold, Temperature *out) {
    FILE *file;
    char file_name[PATH_MAX];
    float temp;

    sprintf(file_name, kTemperatureFileFormat, sensor_num);
    file = fopen(file_name, "r");
    if (file == NULL) {
        PLOG(ERROR) << "readTemperature: failed to open file (" << file_name << ")";
        return -errno;
    }
    if (1 != fscanf(file, "%f", &temp)) {
        fclose(file);
        PLOG(ERROR) << "readTemperature: failed to read a float";
        return errno ? -errno : -EIO;
    }

    fclose(file);

    (*out).type = type;
    (*out).name = name;
    (*out).currentValue = temp * mult;
    (*out).throttlingThreshold = throttling_threshold;
    (*out).shutdownThreshold = shutdown_threshold;
    (*out).vrThrottlingThreshold = vr_throttling_threshold;

    LOG(DEBUG) << android::base::StringPrintf(
        "readTemperature: %d, %d, %s, %g, %g, %g, %g",
        sensor_num, type, name, temp * mult, throttling_threshold,
        shutdown_threshold, vr_throttling_threshold);

    return 0;
}

/**
 * Reads device trip information.
 *
 * @param sensor_num index of thermal zone associated to sensor.
 * @param trip_num index of trip associated to sensor.
 * @param trip_name index of trip associated to sensor.
 * @param mult Multiplier used to translate temperature to Celsius.
 * @param out Pointer to temperature_t structure that will be filled with current
 *     values.
 *
 * @return 0 on success or negative value -errno on error.
 */
static ssize_t readTrip(int sensor_num, int trip_num, const char *trip_name,
                                float mult, Temperature *out) {
    FILE *file;
    char file_name[PATH_MAX];
    float temp;

    sprintf(file_name, kTripTempFileFormat, sensor_num, trip_num);
    file = fopen(file_name, "r");
    if (file == NULL) {
        PLOG(ERROR) << "readTrip: failed to open file (" << file_name << ")";
        return -errno;
    }
    if (1 != fscanf(file, "%f", &temp)) {
        fclose(file);
        PLOG(ERROR) << "readTrip: failed to read a float";
        return errno ? -errno : -EIO;
    }
    fclose(file);

    if (strcmp(trip_name,"threshold") == 0) {
        (*out).throttlingThreshold = temp * mult;
        (*out).vrThrottlingThreshold = temp * mult;
    } else if (strcmp(trip_name,"shutdown") == 0) {
        (*out).shutdownThreshold = temp * mult;
    }

    return 0;
}

static ssize_t getCpuTemperatures(hidl_vec<Temperature> *temperatures) {
    size_t cpu;
    ssize_t result = 0;

    for (cpu = 0; cpu < kCpuNum; cpu++) {
        if (cpu >= temperatures->size()) {
            break;
        }
        // temperature in decidegrees Celsius.
        if (gThermalConfig[cpu+THERMAL_CPU_INDEX].stub) {
            (*temperatures)[cpu].type = TemperatureType::CPU;
            (*temperatures)[cpu].name = kCpuLabel[cpu];
            (*temperatures)[cpu].currentValue = gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold - 10;
            (*temperatures)[cpu].throttlingThreshold = gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold;
            (*temperatures)[cpu].shutdownThreshold = gThermalConfig[cpu+THERMAL_CPU_INDEX].shutdown;
            (*temperatures)[cpu].vrThrottlingThreshold = gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold_vr_min;
        } else {
            result = readTemperature(gThermalConfig[cpu+THERMAL_CPU_INDEX].index,
                                    TemperatureType::CPU,
                                    kCpuLabel[cpu],
                                    0.001,
                                    gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold,
                                    gThermalConfig[cpu+THERMAL_CPU_INDEX].shutdown,
                                    gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold_vr_min,
                                    &(*temperatures)[cpu]);
            if (result != 0) {
                return result;
            }
            if ((gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold == 0) &&
                (gThermalConfig[cpu+THERMAL_CPU_INDEX].shutdown == 0) &&
                (gThermalConfig[cpu+THERMAL_CPU_INDEX].threshold_vr_min == 0)) {
                for (int tripid = 0; tripid < THERMAL_TRIP_MAX_NB; tripid++) {
                    if (gThermalConfig[cpu+THERMAL_CPU_INDEX].trip[tripid].valid) {
                        result = readTrip(gThermalConfig[cpu+THERMAL_CPU_INDEX].index,
                                    gThermalConfig[cpu+THERMAL_CPU_INDEX].trip[tripid].trip_index,
                                    gThermalConfig[cpu+THERMAL_CPU_INDEX].trip[tripid].trip_name,
                                    0.1,
                                    &(*temperatures)[cpu]);
                        if (result != 0) {
                            return result;
                        }
                    }
                }
            }
        }
    }
    return cpu;
}

ssize_t fillTemperatures(hidl_vec<Temperature> *temperatures) {
    ssize_t result = 0;
    size_t current_index = 0;

    if (temperatures == NULL || temperatures->size() < kTemperatureNum) {
        LOG(ERROR) << "fillTemperatures: incorrect buffer";
        return -EINVAL;
    }

    result = getCpuTemperatures(temperatures);
    if (result < 0) {
        return result;
    }
    current_index += result;

    // GPU temperature.
    if (current_index < temperatures->size()) {
        // temperature in decidegrees Celsius.
        if (gThermalConfig[THERMAL_GPU_INDEX].stub) {
            (*temperatures)[current_index].type = TemperatureType::GPU;
            (*temperatures)[current_index].name = kGpuLabel;
            (*temperatures)[current_index].currentValue = gThermalConfig[THERMAL_GPU_INDEX].threshold - 10;
            (*temperatures)[current_index].throttlingThreshold = NAN;
            (*temperatures)[current_index].shutdownThreshold = NAN;
            (*temperatures)[current_index].vrThrottlingThreshold = NAN;
        } else {
            result = readTemperature(gThermalConfig[THERMAL_GPU_INDEX].index,
                                    TemperatureType::GPU,
                                    kGpuLabel,
                                    0.001,
                                    NAN,
                                    NAN,
                                    NAN,
                                    &(*temperatures)[current_index]);
            if (result < 0) {
                return result;
            }
        }
        current_index++;
    }

    // Battery temperature.
    if (current_index < temperatures->size()) {
        // battery: temperature in millidegrees Celsius.
        if (gThermalConfig[THERMAL_BATTERY_INDEX].stub) {
            (*temperatures)[current_index].type = TemperatureType::BATTERY;
            (*temperatures)[current_index].name = kBatteryLabel;
            (*temperatures)[current_index].currentValue = gThermalConfig[THERMAL_BATTERY_INDEX].threshold - 10;
            (*temperatures)[current_index].throttlingThreshold = NAN;
            (*temperatures)[current_index].shutdownThreshold = gThermalConfig[THERMAL_BATTERY_INDEX].shutdown;
            (*temperatures)[current_index].vrThrottlingThreshold = NAN;
        } else {
            result = readTemperature(gThermalConfig[THERMAL_BATTERY_INDEX].index,
                                    TemperatureType::BATTERY,
                                    kBatteryLabel,
                                    0.001,
                                    NAN,
                                    gThermalConfig[THERMAL_BATTERY_INDEX].shutdown,
                                    NAN,
                                    &(*temperatures)[current_index]);
            if (result < 0) {
                return result;
            }
        }
        current_index++;
    }

    // Skin temperature.
    if (current_index < temperatures->size()) {
        // temperature in Celsius.
        if (gThermalConfig[THERMAL_SKIN_INDEX].stub) {
            (*temperatures)[current_index].type = TemperatureType::SKIN;
            (*temperatures)[current_index].name = kSkinLabel;
            (*temperatures)[current_index].currentValue = gThermalConfig[THERMAL_SKIN_INDEX].threshold - 10;
            (*temperatures)[current_index].throttlingThreshold = NAN;
            (*temperatures)[current_index].shutdownThreshold = gThermalConfig[THERMAL_SKIN_INDEX].shutdown;
            (*temperatures)[current_index].vrThrottlingThreshold = NAN;
        } else {
            result = readTemperature(gThermalConfig[THERMAL_SKIN_INDEX].index,
                                    TemperatureType::SKIN,
                                    kSkinLabel,
                                    0.1,
                                    gThermalConfig[THERMAL_SKIN_INDEX].threshold,
                                    gThermalConfig[THERMAL_SKIN_INDEX].shutdown,
                                    gThermalConfig[THERMAL_SKIN_INDEX].threshold_vr_min,
                                    &(*temperatures)[current_index]);
            if (result < 0) {
                return result;
            }
            if ((gThermalConfig[THERMAL_SKIN_INDEX].threshold == 0) &&
                (gThermalConfig[THERMAL_SKIN_INDEX].shutdown == 0) &&
                (gThermalConfig[THERMAL_SKIN_INDEX].threshold_vr_min == 0)) {
                for (int tripid = 0; tripid < THERMAL_TRIP_MAX_NB; tripid++) {
                    if (gThermalConfig[THERMAL_SKIN_INDEX].trip[tripid].valid) {
                        result = readTrip(gThermalConfig[THERMAL_SKIN_INDEX].index,
                                    gThermalConfig[THERMAL_SKIN_INDEX].trip[tripid].trip_index,
                                    gThermalConfig[THERMAL_SKIN_INDEX].trip[tripid].trip_name,
                                    0.1,
                                    &(*temperatures)[current_index]);
                        if (result != 0) {
                            return result;
                        }
                    }
                }
            }
        }
        current_index++;
    }
    return kTemperatureNum;
}

ssize_t fillCpuUsages(hidl_vec<CpuUsage> *cpuUsages) {
    int vals, cpu_num, online;
    ssize_t read;
    uint64_t user, nice, system, idle, active, total;
    char *line = NULL;
    size_t len = 0;
    size_t size = 0;
    char file_name[PATH_MAX];
    FILE *file;
    FILE *cpu_file;

    if (cpuUsages == NULL || cpuUsages->size() < kCpuNum ) {
        LOG(ERROR) << "fillCpuUsages: incorrect buffer";
        return -EINVAL;
    }

    file = fopen(kCpuUsageFile, "r");
    if (file == NULL) {
        PLOG(ERROR) << "fillCpuUsages: failed to open file (" << kCpuUsageFile << ")";
        return -errno;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        // Skip non "cpu[0-9]" lines.
        if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
            free(line);
            line = NULL;
            len = 0;
            continue;
        }

        vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                &nice, &system, &idle);

        free(line);
        line = NULL;
        len = 0;

        if (vals != 5 || size == kCpuNum) {
            if (vals != 5) {
                PLOG(ERROR) << "fillCpuUsages: failed to read CPU information from file ("
                            << kCpuUsageFile << ")";
            } else {
                PLOG(ERROR) << "fillCpuUsages: file has incorrect format ("
                            << kCpuUsageFile << ")";
            }
            fclose(file);
            return errno ? -errno : -EIO;
        }

        active = user + nice + system;
        total = active + idle;

        // Read online CPU information.
        snprintf(file_name, PATH_MAX, kCpuOnlineFileFormat, cpu_num);
        cpu_file = fopen(file_name, "r");
        online = 0;
        if (cpu_file == NULL) {
            PLOG(WARNING) << "fillCpuUsages: failed to open file (" << file_name << "), consider always online";
            online=1;
        } else {
            if (1 != fscanf(cpu_file, "%d", &online)) {
                PLOG(ERROR) << "fillCpuUsages: failed to read CPU online information from file ("
                            << file_name << ")";
                fclose(file);
                fclose(cpu_file);
                return errno ? -errno : -EIO;
            }
            fclose(cpu_file);
        }

        (*cpuUsages)[size].name = kCpuLabel[size];
        (*cpuUsages)[size].active = active;
        (*cpuUsages)[size].total = total;
        (*cpuUsages)[size].isOnline = static_cast<bool>(online);

        LOG(DEBUG) << "fillCpuUsages: "<< kCpuLabel[size] << ": "
                   << active << " " << total << " " <<  online;
        size++;
    }
    fclose(file);

    if (size != kCpuNum) {
        PLOG(ERROR) << "fillCpuUsages: file has incorrect format (" << kCpuUsageFile << ")";
        return -EIO;
    }
    return kCpuNum;
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android

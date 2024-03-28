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

#include <sys/stat.h>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>

#include "thermal-helper.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;

// If true, stub values returned if not available on kernel side (only for managed types)
constexpr const bool kThermalZoneStub = true;
constexpr const bool kCoolingDeviceStub = true;

static thermal_zone_t gThermalZone;
static cooling_device_t gCoolingDevice;

/* ---------------------------------------------------------- */
/* Managed temperature types = CPU0, CPU1, GPU, BATTERY, SKIN */
/* ---------------------------------------------------------- */

// Temperature names
constexpr const char *kTemperatureName[kTemperatureNum] = 
    {"CPU0", "CPU1", "GPU", "BATTERY", "SKIN"};

// Temperature type associated with temperature names
constexpr const TemperatureType kTemperatureType[kTemperatureNum] = 
    {TemperatureType::CPU, TemperatureType::CPU, TemperatureType::GPU,
        TemperatureType::BATTERY, TemperatureType::SKIN};

// Kernel thermal zone type associated with temperature names (none = no driver)
constexpr const char *kThermalZoneType[kTemperatureNum] = 
    {"cpu0-thermal", "cpu1-thermal", "cpu0-thermal", "dummy-battery", "none"};

// Temperature threshold associated with temperature names
static int gThermalThresholdSize = 0;
static std::vector<TemperatureThreshold> gThermalThreshold = {
    {
        .type = TemperatureType::UNKNOWN,
        .name = "none",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = NAN,
    },
    {
        .type = TemperatureType::UNKNOWN,
        .name = "none",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = NAN,
    },
    {
        .type = TemperatureType::UNKNOWN,
        .name = "none",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = NAN,
    },
    {
        .type = TemperatureType::UNKNOWN,
        .name = "none",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = NAN,
    },
    {
        .type = TemperatureType::UNKNOWN,
        .name = "none",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = NAN,
    }
};

/* ThrottlingSeverity: NONE, LIGHT, MODERATE, SEVERE, CRITICAL, EMERGENCY, SHUTDOWN */
constexpr const int kSeverityNum = static_cast<int>(ThrottlingSeverity::SHUTDOWN) + 1;
constexpr const char *kSeverityThreshold[kSeverityNum] = 
    {"none", "active0", "active1", "passive", "critical", "emergency", "shutdown"};

/* Case V1_0::IThermal */

static const Temperature_1_0 kTempStub_1_0 = {
        .type = static_cast<::android::hardware::thermal::V1_0::TemperatureType>(
                TemperatureType::SKIN),
        .name = "stub thermal zone",
        .currentValue = 35.0,
        .throttlingThreshold = 40.0,
        .shutdownThreshold = 55.0,
        .vrThrottlingThreshold = NAN,
};

/* Case V2_0::IThermal */

static const Temperature_2_0 kTempStub_2_0 = {
        .type = TemperatureType::SKIN,
        .name = "stub thermal zone",
        .value = 35.0,
        .throttlingStatus = ThrottlingSeverity::NONE,
};

static const TemperatureThreshold kTempThresholdStub = {
        .type = TemperatureType::SKIN,
        .name = "stub thermal zone",
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 40.0, 55.0, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .vrThrottlingThreshold = NAN,
};

/* --------------------------------------- */
/* Managed cooling device types = FAN, CPU */
/* --------------------------------------- */

/* Case V1_0::CoolingDevice */

// Cooling names
constexpr const char *kCoolingName_1_0 = "FAN";

// Cooling type associated with cooling names
constexpr const CoolingType_1_0 kCoolingType_1_0 = CoolingType_1_0::FAN_RPM;

// Kernel cooling device type associated with cooling names (none = no driver)
constexpr const char *kCoolingDeviceType_1_0 = "thermal-cpufreq-0";

// Stub values (case kCoolingDeviceStub enabled and no cooling device)
static const CoolingDevice_1_0 kCoolingStub_1_0 = {
        .type = CoolingType_1_0::FAN_RPM,
        .name = "stub cooling device",
        .currentValue = 100.0,
};

/* Case V2_0::CoolingDevice */

// Cooling names
constexpr const char *kCoolingName_2_0[kCoolingNum_2_0] = 
    {"FAN", "CPU"};

// Cooling type associated with cooling names
constexpr const CoolingType_2_0 kCoolingType_2_0[kCoolingNum_2_0] =
    {CoolingType_2_0::FAN, CoolingType_2_0::CPU};

// Kernel cooling device type associated with cooling names (none = no driver)
constexpr const char *kCoolingDeviceType_2_0[kCoolingNum_2_0] = 
    {"none", "thermal-cpufreq-0"};

// Stub values (case kCoolingDeviceStub enabled and no cooling device)
static const CoolingDevice_2_0 kCoolingStub_2_0 = {
        .type = CoolingType_2_0::FAN,
        .name = "stub cooling device",
        .value = 100,
};

// Generic helper methods

/**
 * Reads device temperature.
 *
 * @param thermal_zone_num Instance of the thermal zone
 * @param mult Multiplier used to translate temperature to Celsius
 * @param out Pointer to temperature read
 *
 * @return 0 on success or negative value -errno on error.
 */
static ssize_t readTemperature(int thermal_zone_num, float mult, float *out) {
    FILE *file;
    char file_name[PATH_MAX];
    float temp;

    sprintf(file_name, kThermalZoneTempFileFormat, thermal_zone_num);
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

    *out = temp * mult;

    return 0;
}

/**
 * Reads device trip information.
 *
 * @param thermal_zone_num Instance of the thermal zone
 * @param trip_num Instance of the trip value
 * @param mult Multiplier used to translate temperature to Celsius
 * @param out Pointer to temperature read
 *
 * @return 0 on success or negative value -errno on error.
 */
static ssize_t readTrip(int thermal_zone_num, int trip_num, float mult, float *out) {
    FILE *file;
    char file_name[PATH_MAX];
    float temp;

    sprintf(file_name, kTripTempFileFormat, thermal_zone_num, trip_num);
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

    *out = temp * mult;

    return 0;
}

/**
 * Reads cooling device state.
 *
 * @param cooling_num Instance of the cooling device
 * @param out Pointer to cooling device state read
 *
 * @return 0 on success or negative value -errno on error.
 */
static ssize_t readCoolingDeviceState(int cooling_num, float *out) {
    FILE *file;
    char file_name[PATH_MAX];
    float state;

    sprintf(file_name, kCoolingDeviceCurStateFileFormat, cooling_num);
    file = fopen(file_name, "r");
    if (file == NULL) {
        PLOG(ERROR) << "readTemperature: failed to open file (" << file_name << ")";
        return -errno;
    }
    if (1 != fscanf(file, "%f", &state)) {
        fclose(file);
        PLOG(ERROR) << "readTemperature: failed to read a float";
        return errno ? -errno : -EIO;
    }

    fclose(file);

    *out = state;

    return 0;
}

static bool scanThermalZone();
static bool initTemperatureThreshold();
static bool scanCoolingDevice();

/**
 * Initialization constants based on platform
 *
 * @return true on success or false on error.
 */
bool initThermal() {
    bool res;

    // Scan thermal zone sysfs directories
    res = scanThermalZone();
    if (!res)
        return false;

    // Scan cooling device sysfs directories
    res = scanCoolingDevice();
    if (!res)
        return false;

    // Initialize temperature thresholds with values read from kernel drivers
    res = initTemperatureThreshold();
    if (!res)
        return false;

    return true;
}

/**
 * Scan sysfs thermal zone directories
 *
 * @return true on success or false on error.
 */
static bool scanThermalZone() {
    FILE *file;
    char name[PATH_MAX];
    char type[32];
    struct stat st;

    for (int i=0;i<kMaxThermalZones;i++) {
        sprintf(name, kThermalZoneTypeFileFormat, i);
        if (!stat(name, &st)) {
            // read thermal zone type
            file = fopen(name, "r");
            if (file != NULL) {
                if (1 == fscanf(file, "%s", type)) {
                    strcpy(gThermalZone.zone_type[i], type);
                }
                fclose(file);
            } else {
                // error during scan operation
                return false;
            }
            for (int j=0;j<kMaxThermalTrip;j++) {
                sprintf(name, kTripTypeFileFormat, i, j);
                if (!stat(name, &st)) {
                    // read thermal zone trip type
                    file = fopen(name, "r");
                    if (file != NULL) {
                        if (1 == fscanf(file, "%s", type)) {
                            strcpy(gThermalZone.trip[i].trip_type[j], type);
                        }
                        fclose(file);
                    } else {
                        // error during scan operation
                        return false;
                    }
                } else {
                    gThermalZone.trip[i].nb_trip = j;
                    break;
                }
            }
        } else {
            gThermalZone.nb_zone = i;
            break;
        }
    }
    return true;
}

/**
 * Scan sysfs cooling device directories
 *
 * @return true on success or false on error.
 */
static bool scanCoolingDevice() {
    FILE *file;
    char name[PATH_MAX];
    char type[32];
    struct stat st;

    for (int i=0;i<kMaxCoolingDevices;i++) {
        sprintf(name, kCoolingDeviceTypeFileFormat, i);
        if (!stat(name, &st)) {
            // read thermal zone type
            file = fopen(name, "r");
            if (file != NULL) {
                if (1 == fscanf(file, "%s", type)) {
                    strcpy(gCoolingDevice.cooling_type[i], type);
                }
                fclose(file);
            } else {
                // error during scan operation
                return false;
            }
        } else {
            gCoolingDevice.nb_cooling = i;
            break;
        }
    }
    return true;
}

/**
 * Get back Severity associated to kernel trip type
 *
 * @return index
 */
static int getSeverityIndex(char* trip_type) {
    for (int i=0; i < kSeverityNum; i++) {
        if (strcmp(kSeverityThreshold[i], trip_type) == 0 ) {
            return i;
        }
    }
    return -1;
}

/**
 * Initialize temperature thresholds based on read kernel trip values
 *
 * @return true on success or false on error.
 */
static bool initTemperatureThreshold() {
    int num = 0;
    float value;

    for (int i=0; i < gThermalZone.nb_zone; i++) {
        for (int k=0; k < kTemperatureNum; k++) {
            if (strcmp(gThermalZone.zone_type[i], kThermalZoneType[k]) == 0) {
                gThermalThreshold[num].type = kTemperatureType[k];
                gThermalThreshold[num].name = kTemperatureName[k];
                for (int j=0; j < gThermalZone.trip[i].nb_trip; j++) {
                    if (0 == readTrip(i, j, 0.0001, &value)) {
                        int index = getSeverityIndex(gThermalZone.trip[i].trip_type[j]);
                        if (index < 0) {
                            LOG(WARNING) << "initTemperatureThreshold: unknown trip type " << gThermalZone.trip[i].trip_type[j];
                        } else {
                            gThermalThreshold[num].hotThrottlingThresholds[index] = value;
                        }
                    } else {
                        return false;
                    }
                }
                num++;
            }
        }
    }

    gThermalThresholdSize = num;
    return true;
}

// Helper methods for ::android::hardware::thermal::V2_0::IThermal follow.

/**
 * Fill temperature for all available sensors
 * 
 * @param temperatures Pointer to temperatures data
 *
 * @return number of data returned
 */
ssize_t fillTemperatures_2_0(std::vector<Temperature_2_0> *temperatures) {
    ssize_t num = 0;
    float value;

    if (temperatures == NULL || temperatures->size() < kTemperatureNum) {
        LOG(ERROR) << "fillTemperatures_2_0: incorrect buffer";
        return 0;
    }

    if (gThermalZone.nb_zone == 0) {
        if (kThermalZoneStub) {
            temperatures->clear();
            temperatures->insert(temperatures->begin(), kTempStub_2_0);
            return 1;
        }
        LOG(WARNING) << "fillTemperatures_2_0: nb_zone 0 while kThermalZoneStub is false";
        return 0;
    }


    for (int i=0; i < gThermalZone.nb_zone; i++) {
        if (0 == readTemperature(i, 0.0001, &value)) {
            for (int j=0; j < kTemperatureNum; j++) {
                if (strcmp(gThermalZone.zone_type[i], kThermalZoneType[j]) == 0) {
                    (*temperatures)[num].type = kTemperatureType[j];
                    (*temperatures)[num].name = kTemperatureName[j];
                    (*temperatures)[num].value = value;
                    // TODO: add management of throttling status
                    (*temperatures)[num].throttlingStatus = ThrottlingSeverity::NONE;
                    num++;
                }
            }
        }
    }

    return num;
}

/**
 * Fill temperature for all sensors associated to expected temperature type
 * 
 * @param temperatures Pointer to temperature data
 * @param type type of temperature required
 *
 * @return number of data returned
 */
ssize_t fillTemperature_2_0(std::vector<Temperature_2_0> *temperatures, TemperatureType type) {
    ssize_t num = 0;
    float value;

    if (temperatures == NULL || temperatures->size() < kTemperatureNum) {
        LOG(ERROR) << "fillTemperature_2_0: incorrect buffer";
        return 0;
    }

    if (gThermalZone.nb_zone == 0) {
        if ((kThermalZoneStub) && (type == kTempStub_2_0.type)) {
            temperatures->clear();
            temperatures->insert(temperatures->begin(), kTempStub_2_0);
            return 1;
        }
        LOG(WARNING) << "fillTemperature_2_0: nb_zone 0 while kThermalZoneStub is false";
        return 0;
    }

    for (int i=0; i < gThermalZone.nb_zone; i++) {
        if (0 == readTemperature(i, 0.0001, &value)) {
            for (int j=0; j < kTemperatureNum; j++) {
                if (strcmp(gThermalZone.zone_type[i], kThermalZoneType[j]) == 0) {
                    if (type == kTemperatureType[j]) {
                        (*temperatures)[num].type = kTemperatureType[j];
                        (*temperatures)[num].name = kTemperatureName[j];
                        (*temperatures)[num].value = value;
                        // TODO: add management of throttling status
                        (*temperatures)[num].throttlingStatus = ThrottlingSeverity::NONE;
                        num++;
                    }
                }
            }
        }
    }

    return num;
}

/**
 * Fill temperature thresholds associated to all available sensors
 * 
 * @param temperature_thresholds Pointer to temperature thresholds data
 *
 * @return number of data returned
 */
ssize_t fillTemperaturesThreshold(std::vector<TemperatureThreshold> *temperature_thresholds) {

    if (gThermalZone.nb_zone == 0) {
        if (kThermalZoneStub) {
            temperature_thresholds->clear();
            temperature_thresholds->insert(temperature_thresholds->begin(), kTempThresholdStub);
            return 1;
        }
        return 0;
    }

    temperature_thresholds = &gThermalThreshold;

    return gThermalThresholdSize;
}

/**
 * Fill temperature thresholds associated to sensors of the exepected type
 * 
 * @param temperature_thresholds Pointer to temperature thresholds data
 *
 * @return number of data returned
 */
ssize_t fillTemperatureThreshold(std::vector<TemperatureThreshold> *temperature_thresholds, TemperatureType type) {
    int index = 0;

    temperature_thresholds->clear();

    if (gThermalZone.nb_zone == 0) {
        if ((kThermalZoneStub) && (type == kTempThresholdStub.type)) {
            temperature_thresholds->insert(temperature_thresholds->begin(), kTempThresholdStub);
            return 1;
        }
        return 0;
    }

    for (int i=0; i < gThermalThresholdSize; i++) {
        if (gThermalThreshold[i].type == type) {
            temperature_thresholds->insert(temperature_thresholds->begin() + index, gThermalThreshold[i]);
            index++;
        }
    }

    return index;
}

/**
 * Fill states for all available cooling devices
 * 
 * @param cooling_device Pointer to cooling device data
 *
 * @return number of data returned
 */
ssize_t fillCoolingDevices_2_0(std::vector<CoolingDevice_2_0> *cooling_device) {
    ssize_t num = 0;
    float value;

    if (cooling_device == NULL) {
        LOG(ERROR) << "fillCoolingDevices_2_0: incorrect buffer";
        return 0;
    }

    if (gCoolingDevice.nb_cooling == 0) {
        if (kCoolingDeviceStub) {
            cooling_device->clear();
            cooling_device->insert(cooling_device->begin(), kCoolingStub_2_0);
            return 1;
        }
        return 0;
    }

    for (int i=0; i < gCoolingDevice.nb_cooling; i++) {
        if (0 == readCoolingDeviceState(i, &value)) {
            for (int j=0; j < kCoolingNum_2_0; j++) {
                if (strcmp(gCoolingDevice.cooling_type[i], kCoolingDeviceType_2_0[j]) == 0) {
                    (*cooling_device)[num].type = kCoolingType_2_0[j];
                    (*cooling_device)[num].name = kCoolingName_2_0[j];
                    (*cooling_device)[num].value = value;
                    num++;
                }
            }
        }
    }

    return num;
}

/**
 * Fill states for all cooling devices associated to the expected type
 * 
 * @param cooling_device Pointer to cooling device data
 * @param type Cooling device type expected
 *
 * @return number of data returned
 */
ssize_t fillCoolingDevice_2_0(std::vector<CoolingDevice_2_0> *cooling_device, CoolingType_2_0 type) {
    ssize_t num = 0;
    float value;

    if (cooling_device == NULL || cooling_device->size() < kCoolingNum_2_0) {
        LOG(ERROR) << "fillCoolingDevices_2_0: incorrect buffer";
        return 0;
    }

    if (gCoolingDevice.nb_cooling == 0) {
        if ((kCoolingDeviceStub) && (type == kCoolingStub_2_0.type)) {
            cooling_device->clear();
            cooling_device->insert(cooling_device->begin(), kCoolingStub_2_0);
            return 1;
        }
        return 0;
    }

    for (int i=0; i < gCoolingDevice.nb_cooling; i++) {
        if (0 == readCoolingDeviceState(i, &value)) {
            for (int j=0; j < kCoolingNum_2_0; j++) {
                if (type == kCoolingType_2_0[j]) {
                    (*cooling_device)[num].type = kCoolingType_2_0[j];
                    (*cooling_device)[num].name = kCoolingName_2_0[j];
                    (*cooling_device)[num].value = value;
                    num++;
                }
            }
        }
    }

    return num;
}

// Helper methods for ::android::hardware::thermal::V1_0::IThermal follow.

/**
 * Fill temperature for all available sensors
 * 
 * @param temperatures Pointer to temperature data
 *
 * @return number of data returned
 */
ssize_t fillTemperatures_1_0(std::vector<Temperature_1_0> *temperatures) {
    ssize_t num = 0;
    float value;

    if (temperatures == NULL || temperatures->size() < kTemperatureNum) {
        LOG(ERROR) << "fillTemperatures_1_0: incorrect buffer";
        return 0;
    }

    if (gThermalZone.nb_zone == 0) {
        if (kThermalZoneStub) {
            temperatures->clear();
            temperatures->insert(temperatures->begin(), kTempStub_1_0);
            return 1;
        }
        LOG(WARNING) << "fillTemperatures_1_0: nb_zone 0 while kThermalZoneStub is false";
        return 0;
    }

    for (int i=0; i < gThermalZone.nb_zone; i++) {
        if (0 == readTemperature(i, 0.0001, &value)) {
            for (int j=0; j < kTemperatureNum; j++) {
                if (strcmp(gThermalZone.zone_type[i], kThermalZoneType[j]) == 0) {
                    (*temperatures)[num].type = static_cast<::android::hardware::thermal::V1_0::TemperatureType>(kTemperatureType[j]);
                    (*temperatures)[num].name = kTemperatureName[j];
                    (*temperatures)[num].currentValue = value;
                    (*temperatures)[num].throttlingThreshold = gThermalThreshold[j].hotThrottlingThresholds[static_cast<int>(ThrottlingSeverity::SEVERE)];
                    // Use critical temperature as shutdown threshold (current kernel configuration)
                    (*temperatures)[num].shutdownThreshold = gThermalThreshold[j].hotThrottlingThresholds[static_cast<int>(ThrottlingSeverity::CRITICAL)];
                    (*temperatures)[num].vrThrottlingThreshold = gThermalThreshold[j].vrThrottlingThreshold;
                    num++;
                }
            }
        }
    }

    return num;
}

/**
 * Fill state for all available cooling devices
 * 
 * @param cooling_device Pointer to cooling device data
 *
 * @return number of data returned
 */
ssize_t fillCoolingDevices_1_0(std::vector<CoolingDevice_1_0> *cooling_device) {
    ssize_t num = 0;
    float value;

    if (cooling_device == NULL) {
        LOG(ERROR) << "fillCoolingDevice_1_0: incorrect buffer";
        return 0;
    }

    if (gCoolingDevice.nb_cooling == 0) {
        if (kCoolingDeviceStub) {
            cooling_device->clear();
            cooling_device->insert(cooling_device->begin(), kCoolingStub_1_0);
            return 1;
        }
        return 0;
    }

    for (int i=0; i < gCoolingDevice.nb_cooling; i++) {
        if (0 == readCoolingDeviceState(i, &value)) {
            if (strcmp(gCoolingDevice.cooling_type[i], kCoolingDeviceType_1_0) == 0) {
                (*cooling_device)[num].type = kCoolingType_1_0;
                (*cooling_device)[num].name = kCoolingName_1_0;
                (*cooling_device)[num].currentValue = value;
                num++;
                break;
            }
        }
    }

    return num;
}

/**
 * Fill CPU usage
 * 
 * @param cpuUsages Pointer to CPU usage data
 *
 * @return number of data returned
 */
ssize_t fillCpuUsages(std::vector<CpuUsage> *cpuUsages) {
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

        (*cpuUsages)[size].name = kTemperatureName[size];
        (*cpuUsages)[size].active = active;
        (*cpuUsages)[size].total = total;
        (*cpuUsages)[size].isOnline = static_cast<bool>(online);

        LOG(DEBUG) << "fillCpuUsages: "<< kTemperatureName[size] << ": "
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
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

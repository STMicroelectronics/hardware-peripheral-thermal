/*
 * Copyright (C) 2016 STMicroelectronics
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

#ifndef THERMAL_CONFIG_H
#define THERMAL_CONFIG_H

#define THERMAL_NAME_MAX_SIZE    30
#define THERMAL_TYPE_MAX_SIZE    30

#define THERMAL_TRIP_MAX_NB      3

/* Maximum configuration = 2xCPU, 1xGPU, 1xBATTERY, 1xSKIN = 5 */
#define THERMAL_CONFIG_CPU_MAX   2
#define THERMAL_CONFIG_MAX       THERMAL_CONFIG_CPU_MAX+3

enum thermal_config_index {
    THERMAL_CPU_INDEX = 0,
    THERMAL_GPU_INDEX = THERMAL_CONFIG_CPU_MAX,
    THERMAL_BATTERY_INDEX,
    THERMAL_SKIN_INDEX,
};


typedef enum {
  THERMAL_SUCCESS = 0,
  THERMAL_ERROR_NONE = 0,
  THERMAL_ERROR_UNKNOWN = -1,
  THERMAL_ERROR_NOT_SUPPORTED = -2,
  THERMAL_ERROR_NOT_AVAILABLE = -3,
  THERMAL_ERROR_INVALID_ARGS = -4,
  THERMAL_ERROR_TIMED_OUT = -5,
} thermal_error;

/* Thermal configuration */

struct thermal_trip_t {
    char            *trip_name;
    char            *trip_type;
    int             trip_index;
    bool            valid; // is it fixed trip values ?
};

struct thermal_config_t {
    char            *name;
    char            *type;
    int             index;
    float           threshold;
    float           shutdown;
    float           threshold_vr_min;
    struct thermal_trip_t  trip[THERMAL_TRIP_MAX_NB];
    bool            stub; // is it a stubbed interface ?
    bool            fixed; // is it fixed trip values ?
};


int parse_config_file(struct thermal_config_t config[THERMAL_CONFIG_MAX]);
int reset_config(char *device);

#endif // THERMAL_CONFIG_H

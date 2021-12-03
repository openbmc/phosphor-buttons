/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include <sdbusplus/bus.hpp>
#include <string>
#include <vector>

// this struct has the gpio config for single gpio
struct gpioInfo
{
    int fd; // io fd mapped with the gpio
    uint32_t number;
    std::string direction;
};

// this struct represents button interface
struct buttonConfig
{
    std::string formFactorName;  // name of the button interface
    std::vector<gpioInfo> gpios; // holds single or group gpio config
                                 // corresponding to button interface
};

/**
 * @brief iterates over the list of gpios and configures gpios them
 * config which is set from gpio defs json file.
 * The fd of the configured gpio is stored in buttonConfig.gpios container
 * @return int returns 0 on successful config of all gpios
 */

int configGroupGpio(sdbusplus::bus::bus& bus, buttonConfig& buttonCfg);

/**
 * @brief  configures and initializes the single gpio
 * @return int returns 0 on successful config of all gpios
 */

int configGpio(sdbusplus::bus::bus& bus, gpioInfo& gpioConfig);

uint32_t getGpioNum(const std::string& gpioPin);
void closeGpio(int fd);

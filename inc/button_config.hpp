#pragma once
#include "config.h"

#include "cpld.hpp"
#include "gpio.hpp"

#include <nlohmann/json.hpp>

#include <iostream>

enum class ConfigType
{
    gpio,
    cpld
};

// this struct represents button interface
struct buttonConfig
{
    ConfigType type;
    std::string formFactorName;   // name of the button interface
    std::vector<gpioInfo> gpios;  // holds single or group gpio config
    cpldInfo cpld;                // holds single cpld config
    std::vector<int> fds;         // store all the fds listen io event which
                                  // mapped with the gpio or cpld
    nlohmann::json extraJsonInfo; // corresponding to button interface
};

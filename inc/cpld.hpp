
#pragma once
#include "config.h"

#include <cstdint>
#include <string>

struct buttonConfig;

struct cpldInfo
{
    std::string registerName;
    uint32_t i2cAddress;
    uint32_t i2cBus;
    int fd; // io fd mapped with the cpld
};

int configCpld(buttonConfig& buttonCfg);

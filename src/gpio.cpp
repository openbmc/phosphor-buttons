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

#include "config.h"

#include "gpio.hpp"

#include <error.h>
#include <fcntl.h>
#include <unistd.h>

#include <gpioplus/utility/aspeed.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>

const std::string gpioDev = "/sys/class/gpio";

namespace fs = std::filesystem;

void closeGpio(int fd)
{
    if (fd > 0)
    {
        ::close(fd);
    }
}

uint32_t getGpioBase()
{
    // Look for a /sys/class/gpio/gpiochip*/label file
    // with a value of GPIO_BASE_LABEL_NAME.  Then read
    // the base value from the 'base' file in that directory.
#ifdef LOOKUP_GPIO_BASE
    for (auto& f : fs::directory_iterator(gpioDev))
    {
        std::string path{f.path()};
        if (path.find("gpiochip") == std::string::npos)
        {
            continue;
        }

        std::ifstream labelStream{path + "/label"};
        std::string label;
        labelStream >> label;

        if (label == GPIO_BASE_LABEL_NAME)
        {
            uint32_t base;
            std::ifstream baseStream{path + "/base"};
            baseStream >> base;
            return base;
        }
    }

    lg2::error("Could not find GPIO base");
    throw std::runtime_error("Could not find GPIO base!");
#else
    return 0;
#endif
}

uint32_t getGpioNum(const std::string& gpioPin)
{
    // gpioplus promises that they will figure out how to easily
    // support multiple BMC vendors when the time comes.
    auto offset = gpioplus::utility::aspeed::nameToOffset(gpioPin);

    return getGpioBase() + offset;
}

int configGroupGpio(buttonConfig& buttonIFConfig)
{
    int result = 0;
    // iterate the list of gpios from the button interface config
    // and initialize them
    for (auto& gpioCfg : buttonIFConfig.gpios)
    {
        result = configGpio(gpioCfg);
        if (result < 0)
        {
            lg2::error("{NAME}: Error configuring gpio-{NUM}: {RESULT}", "NAME",
                       buttonIFConfig.formFactorName, "NUM", gpioCfg.number,
                       "RESULT", result);

            break;
        }
    }

    return result;
}

int configGpio(gpioInfo& gpioConfig)
{
    auto gpioNum = gpioConfig.number;
    auto gpioDirection = gpioConfig.direction;

    std::string devPath{gpioDev};

    std::fstream stream;

    stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    devPath += "/gpio" + std::to_string(gpioNum) + "/value";

    fs::path fullPath(devPath);

    if (fs::exists(fullPath))
    {
        lg2::info("GPIO exported: {PATH}", "PATH", devPath);
    }
    else
    {
        devPath = gpioDev + "/export";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioNum;
            stream.close();
        }

        catch (const std::exception& e)
        {
            lg2::error("{NUM} error in writing {PATH}: {ERROR}", "NUM", gpioNum,
                       "PATH", devPath, "ERROR", e);
            return -1;
        }
    }

    if (gpioDirection == "out")
    {
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

        uint32_t currentValue;

        stream.open(devPath, std::fstream::in);
        try
        {
            stream >> currentValue;
            stream.close();
        }

        catch (const std::exception& e)
        {
            lg2::error("Error in reading {PATH}: {ERROR}", "PATH", devPath,
                       "ERROR", e);
            return -1;
        }

        const char* direction = currentValue ? "high" : "low";

        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/direction";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << direction;
            stream.close();
        }

        catch (const std::exception& e)
        {
            lg2::error("Error in writing: {ERROR}", "ERROR", e);
            return -1;
        }
    }
    else if (gpioDirection == "in")
    {
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/direction";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioDirection;
            stream.close();
        }

        catch (const std::exception& e)
        {
            lg2::error("Error in writing: {ERROR}", "ERROR", e);
            return -1;
        }
    }
    else if ((gpioDirection == "both"))
    {
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/direction";

        stream.open(devPath, std::fstream::out);
        try
        {
            // Before set gpio configure as an interrupt pin, need to set
            // direction as 'in' or edge can't set as 'rising', 'falling' and
            // 'both'
            const char* in_direction = "in";
            stream << in_direction;
            stream.close();
        }

        catch (const std::exception& e)
        {
            lg2::error("Error in writing: {ERROR}", "ERROR", e);
            return -1;
        }
        devPath.clear();

        // For gpio configured as ‘both’, it is an interrupt pin and trigged on
        // both rising and falling signals
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/edge";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioDirection;
            stream.close();
        }

        catch (const std::exception& e)
        {
            lg2::error("Error in writing: {ERROR}", "ERROR", e);
            return -1;
        }
    }

    devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

    auto fd = ::open(devPath.c_str(), O_RDWR | O_NONBLOCK);

    if (fd < 0)
    {
        lg2::error("Open {PATH} error: {ERROR}", "PATH", devPath, "ERROR",
                   errno);
        return -1;
    }

    gpioConfig.fd = fd;

    return 0;
}

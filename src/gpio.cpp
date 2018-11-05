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

#include "gpio.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <experimental/filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

const static constexpr char* SYSMGR_SERVICE = "org.openbmc.managers.System";
const static constexpr char* SYSMGR_OBJ_PATH = "/org/openbmc/managers/System";
const static constexpr char* SYSMGR_INTERFACE = "org.openbmc.managers.System";

static constexpr auto gpioDefs = "/etc/default/obmc/gpio/gpio_defs.json";

using namespace phosphor::logging;

void closeGpio(int fd)
{
    if (fd > 0)
    {
        ::close(fd);
    }
}

bool gpioDefined(const std::string& gpioName)
{
    try
    {
        std::ifstream gpios{gpioDefs};
        auto json = nlohmann::json::parse(gpios, nullptr, true);
        auto defs = json["gpio_definitions"];

        auto gpio =
            std::find_if(defs.begin(), defs.end(), [&gpioName](const auto g) {
                return gpioName == g["name"];
            });

        if (gpio != defs.end())
        {
            return true;
        }
    }
    catch (std::exception& e)
    {
        log<level::ERR>("Error parsing GPIO JSON", entry("ERROR=%s", e.what()),
                        entry("GPIO_NAME=%s", gpioName.c_str()));
    }
    return false;
}

int configGpio(const char* gpioName, int* fd, sdbusplus::bus::bus& bus)
{
    auto method = bus.new_method_call(SYSMGR_SERVICE, SYSMGR_OBJ_PATH,
                                      SYSMGR_INTERFACE, "gpioInit");

    method.append(gpioName);

    auto result = bus.call(method);

    if (result.is_method_error())
    {
        log<level::ERR>("bus call error!");
        return -1;
    }

    int32_t gpioNum;
    std::string gpioDev;
    std::string gpioDirection;

    result.read(gpioDev, gpioNum, gpioDirection);

    std::string devPath;

    std::fstream stream;

    stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    devPath.clear();
    devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

    std::experimental::filesystem::path fullPath(devPath);

    if (std::experimental::filesystem::exists(fullPath))
    {
        log<level::INFO>("GPIO exported", entry("PATH=%s", devPath.c_str()));
    }
    else
    {
        devPath.clear();
        devPath = gpioDev + "/export";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioNum;
            stream.close();
        }

        catch (const std::exception& e)
        {
            log<level::ERR>("Error in writing!",
                            entry("PATH=%s", devPath.c_str()),
                            entry("NUM=%d", gpioNum));
            return -1;
        }
    }

    if (gpioDirection == "out")
    {
        devPath.clear();
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
            log<level::ERR>("Error in reading!",
                            entry("PATH=%s", devPath.c_str()));
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
            log<level::ERR>("Error in writing!");
            return -1;
        }
    }
    else if (gpioDirection == "in")
    {
        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/direction";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioDirection;
            stream.close();
        }

        catch (const std::exception& e)
        {
            log<level::ERR>("Error in writing!");
            return -1;
        }
    }
    else if ((gpioDirection == "both"))
    {

        // For gpio configured as ‘both’, it is an interrupt pin and trigged on
        // both rising and falling signals
        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/edge";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioDirection;
            stream.close();
        }

        catch (const std::exception& e)
        {
            log<level::ERR>("Error in writing!");
            return -1;
        }
    }

    devPath.clear();
    devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

    *fd = ::open(devPath.c_str(), O_RDWR | O_NONBLOCK);

    if (*fd < 0)
    {
        log<level::ERR>("open error!");
        return -1;
    }

    return 0;
}

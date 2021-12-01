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

#include "button_factory.hpp"
#include "gpio.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
static constexpr auto gpioDefFile = "/etc/default/obmc/gpio/gpio_defs.json";

int main(int argc, char* argv[])
{
    int ret = 0;

    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Start power button service...");

    sd_event* event = nullptr;
    ret = sd_event_default(&event);
    if (ret < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error creating a default sd_event handler");
        return ret;
    }
    EventPtr eventP{event};
    event = nullptr;

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    sdbusplus::server::manager::manager objManager{
        bus, "/xyz/openbmc_project/Chassis/Buttons"};

    bus.request_name("xyz.openbmc_project.Chassis.Buttons");

    std::vector<std::unique_ptr<buttonIface>> buttonInterfaces;
    buttonFactory buttonFactoryInstance;

    std::ifstream gpios{gpioDefFile};
    auto json = nlohmann::json::parse(gpios, nullptr, true);
    auto gpioDefs = json["gpio_definitions"];

    // load gpio config from gpio defs json file and create button interface
    // objects based on the button form factor type

    for (auto groupGpioConfig : gpioDefs)
    {
        std::string formFactorName = groupGpioConfig["name"];
        buttonConfig buttonCfg;
        std::unique_ptr<buttonIface> tempButtonIf;
        auto groupGpio = groupGpioConfig["gpio_config"];

        for (auto gpioConfig : groupGpio)
        {
            gpioInfo gpioCfg;
            gpioCfg.number = getGpioNum(gpioConfig["pin"]);
            gpioCfg.direction = gpioConfig["direction"];
            buttonCfg.formFactorName = formFactorName;
            buttonCfg.gpios.push_back(gpioCfg);

            std::string errorMsg = "checkGpios : gpioCfg.number =" +
                                   std::to_string(gpioCfg.number) +
                                   ",gpioCfg.direction =" + gpioCfg.direction;
            phosphor::logging::log<phosphor::logging::level::ERR>(
                errorMsg.c_str());
        }

        tempButtonIf = buttonFactoryInstance.createInstance(formFactorName, bus,
                                                            eventP, buttonCfg);
        buttonInterfaces.push_back(std::move(tempButtonIf));
    }

    try
    {
        bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
        ret = sd_event_loop(eventP.get());
        if (ret < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error occurred during the sd_event_loop",
                phosphor::logging::entry("RET=%d", ret));
        }
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        ret = -1;
    }
    return ret;
}

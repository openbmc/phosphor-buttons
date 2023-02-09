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
#include "button_config.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

#include <fstream>
static constexpr auto gpioDefFile = "/etc/default/obmc/gpio/gpio_defs.json";

nlohmann::json gpioDefs;
nlohmann::json cpldDefs;

int main(void)
{
    int ret = 0;

    lg2::info("Start Phosphor buttons service...");

    sd_event* event = nullptr;
    ret = sd_event_default(&event);
    if (ret < 0)
    {
        lg2::error("Error creating a default sd_event handler");
        return ret;
    }
    EventPtr eventP{event};
    event = nullptr;

    sdbusplus::bus_t bus = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t objManager{
        bus, "/xyz/openbmc_project/Chassis/Buttons"};

    bus.request_name("xyz.openbmc_project.Chassis.Buttons");
    std::vector<std::unique_ptr<ButtonIface>> buttonInterfaces;

    std::ifstream gpios{gpioDefFile};
    auto configDefJson = nlohmann::json::parse(gpios, nullptr, true);
    gpioDefs = configDefJson["gpio_definitions"];
    cpldDefs = configDefJson["cpld_definitions"];

    // load cpld config from gpio defs json file and create button interface
    for (const auto& cpldConfig : cpldDefs)
    {
        std::string formFactorName = cpldConfig["name"];

        buttonConfig buttonCfg;
        buttonCfg.type = ConfigType::cpld;
        buttonCfg.formFactorName = formFactorName;
        buttonCfg.extraJsonInfo = cpldConfig;

        cpldInfo cpldCfg;
        cpldCfg.registerName = cpldConfig["register_name"];

        cpldCfg.i2cAddress = cpldConfig["i2c_address"].get<int>();
        cpldCfg.i2cBus = cpldConfig["i2c_bus"].get<int>();
        buttonCfg.cpld = cpldCfg;

        auto tempButtonIf = ButtonFactory::instance().createInstance(
            formFactorName, bus, eventP, buttonCfg);
        if (tempButtonIf)
        {
            buttonInterfaces.emplace_back(std::move(tempButtonIf));
        }
    }

    // load gpio config from gpio defs json file and create button interface
    // objects based on the button form factor type

    for (const auto& gpioConfig : gpioDefs)
    {
        std::string formFactorName = gpioConfig["name"];
        buttonConfig buttonCfg;
        buttonCfg.formFactorName = formFactorName;
        buttonCfg.extraJsonInfo = gpioConfig;
        buttonCfg.type = ConfigType::gpio;

        /* The folloing code checks if the gpio config read
        from json file is single gpio config or group gpio config,
        based on that further data is processed. */
        lg2::debug("Found button config : {FORM_FACTOR_NAME}",
                   "FORM_FACTOR_NAME", buttonCfg.formFactorName);
        if (gpioConfig.contains("group_gpio_config"))
        {
            const auto& groupGpio = gpioConfig["group_gpio_config"];

            for (const auto& config : groupGpio)
            {
                gpioInfo gpioCfg;
                gpioCfg.number = getGpioNum(config["pin"]);
                gpioCfg.direction = config["direction"];
                gpioCfg.name = config["name"];
                gpioCfg.polarity = (config["polarity"] == "active_high")
                                       ? GpioPolarity::activeHigh
                                       : GpioPolarity::activeLow;
                buttonCfg.gpios.push_back(gpioCfg);
            }
        }
        else
        {
            gpioInfo gpioCfg;
            gpioCfg.number = getGpioNum(gpioConfig["pin"]);
            gpioCfg.direction = gpioConfig["direction"];
            buttonCfg.gpios.push_back(gpioCfg);
        }
        auto tempButtonIf = ButtonFactory::instance().createInstance(
            formFactorName, bus, eventP, buttonCfg);
        /* There are additional gpio configs present in some platforms
         that are not supported in phosphor-buttons.
        But they may be used by other applications. so skipping such configs
        if present in gpio_defs.json file*/
        if (tempButtonIf)
        {
            buttonInterfaces.emplace_back(std::move(tempButtonIf));
        }
    }

    try
    {
        bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
        ret = sd_event_loop(eventP.get());
        if (ret < 0)
        {
            lg2::error("Error occurred during the sd_event_loop : {RESULT}",
                       "RESULT", ret);
        }
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        ret = -1;
    }
    return ret;
}

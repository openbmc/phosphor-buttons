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
#include "button_factory.hpp"
#include "button_interface.hpp"
#include "common.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/HostSelector/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <unistd.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>

const static constexpr char* HOST_SELECTOR = "HOST_SELECTOR";

const static constexpr int INITIAL_HS_POSITION = 6;

const static constexpr int INVALID_INDEX = 0xff;

using namespace phosphor::logging;

class HostSelector : public sdbusplus::server::object::object<
                         sdbusplus::xyz::openbmc_project::Chassis::Buttons::
                             server::HostSelector>,
                     public ButtonIface
{
  public:
    HostSelector(sdbusplus::bus::bus& bus, const char* path, EventPtr& event,
                 buttonConfig& buttonCfg) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::
                HostSelector>(bus, path),
        hostSelectorPosition(0), ButtonIface(bus, event, buttonCfg)
    {
        init();
        std::variant<size_t> hsMaxPositionVariant(size_t(0));
        // read and store the host selector position Map
        for (auto& cfg : gpioDefs)
        {
            if (cfg["name"] == getFormFactorName())
            {
                hsPosMap = cfg["host_selector_map"];
                hsMaxPositionVariant = cfg["max_position"];
                break;
            }
        }

        setPropertyByName("MaxPosition", hsMaxPositionVariant, false);

        setInitialHostSelectorValue();
    }

    ~HostSelector()
    {

        deInit();
    }

    static const std::string getFormFactorName()
    {
        return HOST_SELECTOR;
    }

    static const char* getDbusObjectPath()
    {
        return HS_DBUS_OBJECT_NAME;
    }
    void handleEvent(sd_event_source* es, int fd, uint32_t revents) override;
    size_t getMappedHSConfig(size_t hsPosition);
    int getGpioIndex(int fd);
    void setInitialHostSelectorValue(void);
    bool setHostSelectorValue(int fd, bool gpioState);

  protected:
    size_t hostSelectorPosition;
    // map of read Host selector switch value and corresponding host number
    // value.
    nlohmann::json hsPosMap;
};

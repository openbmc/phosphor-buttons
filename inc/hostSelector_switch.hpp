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

static constexpr std::string_view HOST_SELECTOR = "HOST_SELECTOR";

static constexpr auto INVALID_INDEX = std::numeric_limits<size_t>::max();

enum class GPIO_STATE
{
    HIGH,
    LOW
};

class HostSelector final : public sdbusplus::server::object_t<
                               sdbusplus::xyz::openbmc_project::Chassis::
                                   Buttons::server::HostSelector>,
                           public ButtonIface
{
  public:
    HostSelector(sdbusplus::bus::bus& bus, const char* path, EventPtr& event,
                 buttonConfig& buttonCfg) :
        sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Chassis::
                                        Buttons::server::HostSelector>(
            bus, path, action::defer_emit),
        ButtonIface(bus, event, buttonCfg)
    {
        init();
        // read and store the host selector position Map
        hsPosMap = buttonCfg.extraJsonInfo.at("host_selector_map")
                       .get<std::map<std::string, int>>();
        maxPosition(buttonCfg.extraJsonInfo["max_position"], true);

        setInitialHostSelectorValue();
        emit_object_added();
    }

    ~HostSelector()
    {
        deInit();
    }

    static constexpr std::string_view getFormFactorName()
    {
        return HOST_SELECTOR;
    }

    static const char* getDbusObjectPath()
    {
        return HS_DBUS_OBJECT_NAME;
    }
    void handleEvent(sd_event_source* es, int fd, uint32_t revents) override;
    size_t getMappedHSConfig(size_t hsPosition);
    size_t getGpioIndex(int fd);
    void setInitialHostSelectorValue(void);
    void setHostSelectorValue(int fd, GPIO_STATE state);

  protected:
    size_t hostSelectorPosition = 0;

    // map of read Host selector switch value and corresponding host number
    // value.
    std::map<std::string, int> hsPosMap;
};

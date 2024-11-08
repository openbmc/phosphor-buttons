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
#include "config.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/Reset/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>

static constexpr auto RESET_BUTTON = "RESET_BUTTON";

class ResetButton :
    public sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::Reset>,
    public ButtonIface
{
  public:
    ResetButton(sdbusplus::bus_t& bus, const char* path, EventPtr& event,
                ButtonConfig& buttonCfg) :
        sdbusplus::server::object_t<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::Reset>(
            bus, path),
        ButtonIface(bus, event, buttonCfg)
    {
        init();
    }

    ~ResetButton()
    {
        deInit();
    }

    void simPress() override;

    static constexpr std::string getFormFactorName()
    {
        return RESET_BUTTON;
    }

    static constexpr std::string getDbusObjectPath()
    {
        return RESET_DBUS_OBJECT_NAME;
    }

    void handleEvent(sd_event_source* es, int fd, uint32_t revents) override;
};

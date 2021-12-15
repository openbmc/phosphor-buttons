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
#include "xyz/openbmc_project/Chassis/Buttons/Power/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <unistd.h>

#include <chrono>
#include <phosphor-logging/elog-errors.hpp>

const static constexpr char* POWER_BUTTON = "POWER_BUTTON";

struct PowerButton
    : sdbusplus::server::object::object<
          sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::Power>,
      public buttonIface
{

    PowerButton(sdbusplus::bus::bus& bus, const char* path, EventPtr& event,
                buttonConfig& buttonCfg,
                sd_event_io_handler_t handler = PowerButton::EventHandler) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::Power>(
            bus, path),
        buttonIface(bus, event, buttonCfg)
    {

        int ret = -1;
        callbackHandler = handler;

        // config group gpio based on the gpio defs read from the json file
        ret = configGroupGpio(bus, buttonIFConfig);

        if (ret < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "POWER_BUTTON: failed to config GPIO");
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }
        init();
    }

    virtual ~PowerButton()
    {
        ::closeGpio(buttonIFConfig.gpios[0].fd);
    }

    void simPress() override;
    void simLongPress() override;

    virtual void init();

    template <typename T>
    static const std::string getFormFactorName()
    {
        return POWER_BUTTON;
    }

    static const char* getDbusObjectPath()
    {
        return POWER_DBUS_OBJECT_NAME;
    }
    void updatePressedTime();
    auto getPressTime() const;
    virtual void doEventHandling(sd_event_source* es, int fd, uint32_t revents);

    static int EventHandler(sd_event_source* es, int fd, uint32_t revents,
                            void* userdata);

  protected:
    decltype(std::chrono::steady_clock::now()) pressedTime;
};

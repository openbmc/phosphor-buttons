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
#include "button_interface.hpp"
#include "common.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/ID/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>

const static constexpr char* ID_BUTTON = "ID_BTN";

struct IDButton
    : sdbusplus::server::object::object<
          sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::ID>,
      public buttonIface
{

    IDButton(sdbusplus::bus::bus& bus, const char* path, EventPtr& event,
             buttonConfig& buttonCfg,
             sd_event_io_handler_t handler = IDButton::EventHandler) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::ID>(
            bus, path),
        buttonIface(bus, event, buttonCfg)
    {

        int ret = -1;
        callbackHandler = handler;
        // config gpio
        ret = configGroupGpio(bus, buttonIFconfig);

        if (ret < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                (getFormFactorType() + " : failed to config GPIO").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        init();
    }

    ~IDButton()
    {
        ::closeGpio(buttonIFconfig.gpios[0].fd);
    }
    virtual void init()
    {
        char buf;
        int fd = buttonIFconfig.gpios[0].fd;
        ::read(fd, &buf, sizeof(buf));

        int ret = sd_event_add_io(event.get(), nullptr, fd, EPOLLPRI,
                                  callbackHandler, this);
        if (ret < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                (getFormFactorType() + " : failed to add to event loop")
                    .c_str());
            ::closeGpio(fd);
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }
    }
    void simPress() override;

    template <typename T>
    static std::string getFormFactorName()
    {
        return ID_BUTTON;
    }

    static const char* getDbusObjectPath()
    {
        return ID_DBUS_OBJECT_NAME;
    }
    virtual void doEventHandling(sd_event_source* es, int fd, uint32_t revents)
    {
        int n = -1;
        char buf = '0';
        n = ::lseek(fd, 0, SEEK_SET);

        if (n < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                (getFormFactorType() + " : lseek error!").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        n = ::read(fd, &buf, sizeof(buf));
        if (n < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                (getFormFactorType() + " : read error!").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        if (buf == '0')
        {
            phosphor::logging::log<phosphor::logging::level::DEBUG>(
                (getFormFactorType() + " : pressed").c_str());
            // emit pressed signal
            pressed();
        }
        else
        {
            phosphor::logging::log<phosphor::logging::level::DEBUG>(
                (getFormFactorType() + " : released").c_str());
            // released
            released();
        }
    }
    static int EventHandler(sd_event_source* es, int fd, uint32_t revents,
                            void* userdata)
    {

        int n = -1;
        char buf = '0';

        if (!userdata)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "ID_BUTTON: userdata null!");
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        IDButton* idButton = static_cast<IDButton*>(userdata);

        if (!idButton)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "ID_BUTTON: null pointer!");
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        idButton->doEventHandling(es, fd, revents);
        return 0;
    }
};

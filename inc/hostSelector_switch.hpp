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
#include "xyz/openbmc_project/Chassis/Buttons/HostSelector/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <unistd.h>

#include <iostream>
#include <phosphor-logging/elog-errors.hpp>

const static constexpr char* HOST_SELECTOR = "HOST_SELECTOR";

const static constexpr int INITIAL_HS_POSITION = 6;

const static constexpr int INVALID_INDEX = 0xff;

using namespace phosphor::logging;

struct HostSelector : sdbusplus::server::object::object<
                          sdbusplus::xyz::openbmc_project::Chassis::Buttons::
                              server::HostSelector>,
                      public buttonIface
{

    HostSelector(sdbusplus::bus::bus& bus, const char* path, EventPtr& event,
                 buttonConfig& buttonCfg,
                 sd_event_io_handler_t handler = HostSelector::EventHandler) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::
                HostSelector>(bus, path),
        hostSelectorPosition(0), buttonIface(bus, event, buttonCfg)
    {
        int ret = -1;
        callbackHandler = handler;

        ret = configGroupGpio(bus, buttonIFconfig);
        if (ret < 0)
        {
            log<level::ERR>(
                (getFormFactorType() + "failed to config GPIO").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        init();
    }

    ~HostSelector()
    {
        for (auto gpioconfig : buttonIFconfig.gpios)
        {
            ::closeGpio(gpioconfig.fd);
        }
    }

    /**
     * @brief oem specific initialization can be done under init function.
     * if platform specific initialization is needed then
     * a derived class instance with its own init function to override this
     * default init() method
     */

    virtual void init()
    {
        setInitialHostSelectorValue();
        int ret = -1;
        ret = sd_event_add_io(event.get(), nullptr, buttonIFconfig.gpios[0].fd,
                              EPOLLPRI, callbackHandler, this);
        ret = sd_event_add_io(event.get(), nullptr, buttonIFconfig.gpios[1].fd,
                              EPOLLPRI, callbackHandler, this);
        ret = sd_event_add_io(event.get(), nullptr, buttonIFconfig.gpios[2].fd,
                              EPOLLPRI, callbackHandler, this);
        ret = sd_event_add_io(event.get(), nullptr, buttonIFconfig.gpios[3].fd,
                              EPOLLPRI, callbackHandler, this);

        if (ret < 0)
        {
            log<level::ERR>(
                (getFormFactorType() + "failed to add to event loop").c_str());

            for (auto gpioconfig : buttonIFconfig.gpios)
            {
                ::closeGpio(gpioconfig.fd);
            }
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }
    }

    template <typename T>
    static const std::string getFormFactorName()
    {
        return HOST_SELECTOR;
    }

    static const char* getDbusObjectPath()
    {
        return HS_DBUS_OBJECT_NAME;
    }

    size_t getMappedHSValue()
    {
        size_t adjustedPosition;

        adjustedPosition = (hostSelectorPosition - INITIAL_HS_POSITION) / 2;

        return adjustedPosition;
    }
    int getGpioIndex(int fd)
    {
        for (int index = 0; index < 4; index++)
        {
            if (buttonIFconfig.gpios[index].fd == fd)
            {
                return index;
            }
        }
        return INVALID_INDEX;
    }
    virtual void setInitialHostSelectorValue()
    {
        char buf;
        for (int index = 0; index < 4; index++)
        {
            auto result = ::lseek(buttonIFconfig.gpios[index].fd, 0, SEEK_SET);

            if (result < 0)
            {
                throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                    IOError();
            }

            result = ::read(buttonIFconfig.gpios[index].fd, &buf, sizeof(buf));
            if (result < 0)
            {
                throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                    IOError();
            }

            setHostSelectorValue(buttonIFconfig.gpios[index].fd, (buf == '0'));
        }
    }

    bool setHostSelectorValue(int fd, bool gpioState)
    {
        int pos = getGpioIndex(fd);

        std::cerr << "gpio index value: " << pos << "\n";

        if (pos == INVALID_INDEX)
        {
            return false;
        }

        if (gpioState)
        {
            hostSelectorPosition |= 0xf & (0x1 << pos);
        }
        else
        {
            hostSelectorPosition &= ~(0xf & (0x1 << pos));
        }

        return true;
    }
    /**
     * @brief This method is called from sd-event provided callback function
     * callbackHandler if platform specific event handling is needed then a
     * derived class instance with its specific evend handling logic along with
     * init() function can be created to override the default event handling
     */

    virtual void doEventHandling(sd_event_source* es, int fd, uint32_t revents)
    {
        int n = -1;
        char buf = '0';

        n = ::lseek(fd, 0, SEEK_SET);

        if (n < 0)
        {
            log<level::ERR>((getFormFactorType() + "lseek error!").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        n = ::read(fd, &buf, sizeof(buf));
        if (n < 0)
        {
            log<level::ERR>((getFormFactorType() + "read error!").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        // read the gpio state for the io event received
        bool gpioState = (buf == '0');
        std::string debugMsg;
        debugMsg = "host_selector gpio event for fd : " + std::to_string(fd) +
                   " state - " + (gpioState ? "1" : "0");
        log<level::INFO>(debugMsg.c_str());

        setHostSelectorValue(fd, gpioState);

        std::variant<size_t> hsPositionVariant(getMappedHSValue());

        setPropertyByName("Position", hsPositionVariant, false);
    }
    static int EventHandler(sd_event_source* es, int fd, uint32_t revents,
                            void* userdata)
    {

        if (!userdata)
        {
            log<level::ERR>("userdata null!");
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        HostSelector* hostSelectorSwitch = static_cast<HostSelector*>(userdata);

        if (!hostSelectorSwitch)
        {
            log<level::ERR>("null pointer!");
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        hostSelectorSwitch->doEventHandling(es, fd, revents);
        return 0;
    }

  protected:
    size_t hostSelectorPosition;
};

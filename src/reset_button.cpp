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

#include "reset_button.hpp"

#include "xyz/openbmc_project/Chassis/Buttons/Reset/server.hpp"

// add the button iface class to registry
static buttonIFRegister<ResetButton> buttonRegister;

void ResetButton::simPress()
{
    pressed();
}

void ResetButton::init()
{
    char buf;

    // initialize the button io fd from the buttonConfig
    // which has fd stored when configGroupGpio is called
    int fd = buttonIFConfig.gpios[0].fd;

    ::read(fd, &buf, sizeof(buf));

    int ret = sd_event_add_io(event.get(), nullptr, fd, EPOLLPRI,
                              callbackHandler, this);
    if (ret < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "RESET_BUTTON: failed to add to event loop");
        ::closeGpio(fd);
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
}

void ResetButton::doEventHandling(sd_event_source* es, int fd, uint32_t revents)
{
    int n = -1;
    char buf = '0';

    n = ::lseek(fd, 0, SEEK_SET);

    if (n < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "RESET_BUTTON: lseek error!");
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    n = ::read(fd, &buf, sizeof(buf));
    if (n < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "RESET_BUTTON: read error!");
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    if (buf == '0')
    {
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            "RESET_BUTTON: pressed");
        // emit pressed signal
        pressed();
    }
    else
    {
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            "RESET_BUTTON: released");
        // released
        released();
    }

    return;
}

int ResetButton::EventHandler(sd_event_source* es, int fd, uint32_t revents,
                              void* userdata)
{

    if (!userdata)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "RESET_BUTTON: userdata null!");
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    ResetButton* resetButton = static_cast<ResetButton*>(userdata);

    if (!resetButton)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "RESET_BUTTON: null pointer!");
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    resetButton->doEventHandling(es, fd, revents);
    return 0;
}

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

#include "id_button.hpp"

// add the button iface class to registry
static buttonIFRegister<IDButton> buttonRegister;

void IDButton::simPress()
{
    pressed();
}

void IDButton::init()
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
            (getFormFactorType() + " : failed to add to event loop").c_str());
        ::closeGpio(fd);
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
}

void IDButton::doEventHandling(sd_event_source* es, int fd, uint32_t revents)
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

int IDButton::EventHandler(sd_event_source* es, int fd, uint32_t revents,
                           void* userdata)
{

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
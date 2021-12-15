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
static ButtonIFRegister<IDButton> buttonRegister;

void IDButton::simPress()
{
    pressed();
}

void IDButton::init()
{
    char buf;
    // initialize the button io fd from the buttonConfig
    // which has fd stored when configGroupGpio is called
    int fd = config.gpios[0].fd;

    int ret = ::read(fd, &buf, sizeof(buf));
    if (ret < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            (getFormFactorType() + " : read error!").c_str());
    }

    ret = sd_event_add_io(event.get(), nullptr, fd, EPOLLPRI, callbackHandler,
                          this);
    if (ret < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            (getFormFactorType() + " : failed to add to event loop").c_str());
        ::closeGpio(fd);
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
}

void IDButton::handleEvent(sd_event_source* es, int fd, uint32_t revents)
{
    int n = -1;
    char buf = '0';
    n = ::lseek(fd, 0, SEEK_SET);

    if (n < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            (getFormFactorType() + " : lseek error!").c_str());
        return;
    }

    n = ::read(fd, &buf, sizeof(buf));
    if (n < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            (getFormFactorType() + " : read error!").c_str());
        return;
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

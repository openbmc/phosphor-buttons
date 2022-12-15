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

#include "power_button.hpp"
#include "button_handler.hpp"

// add the button iface class to registry
static ButtonIFRegister<PowerButton> buttonRegister;
static ButtonIFRegister<PowerButton> multiButtonRegister(phosphor::button::numberOfChassis());

void PowerButton::simPress()
{
    pressed();
}

void PowerButton::simLongPress()
{
    pressedLong();
}

void PowerButton::simLongerPress()
{
    pressedLonger();
}

void PowerButton::updatePressedTime()
{
    pressedTime = std::chrono::steady_clock::now();
}

auto PowerButton::getPressTime() const
{
    return pressedTime;
}

void PowerButton::handleEvent(sd_event_source* /* es */, int fd,
                              uint32_t /* revents */)
{
    int n = -1;
    char buf = '0';

    n = ::lseek(fd, 0, SEEK_SET);

    if (n < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "POWER_BUTTON: lseek error!");
        return;
    }

    n = ::read(fd, &buf, sizeof(buf));
    if (n < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "POWER_BUTTON: read error!");
        return;
    }

    if (buf == '0')
    {
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            "POWER_BUTTON: pressed");

        updatePressedTime();
        // emit pressed signal
        pressed();
    }
    else
    {
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            "POWER_BUTTON: released");

        auto now = std::chrono::steady_clock::now();
        auto d = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - getPressTime());

        if (d < std::chrono::milliseconds(LONG_PRESS_TIME_MS))
        {
            released();
        }
        else if (static_cast<std::string>(SUPPORT_LONGER_PRESS) == "true")
        {
            if(d < std::chrono::milliseconds(LONGER_PRESS_TIME_MS))
            {
                pressedLong();
            }
            else
            {
                pressedLonger();
            }
        }
        else
        {
            pressedLong();
        }
    }
}

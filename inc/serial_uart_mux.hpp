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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or 0implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#pragma once
#include "config.h"

#include "button_factory.hpp"
#include "button_interface.hpp"
#include "common.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/HostSelector/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"
#include "xyz/openbmc_project/Inventory/Item/server.hpp"

#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>

static constexpr std::string_view SERIAL_CONSOLE_SWITCH = "SERIAL_UART_MUX";
enum SerialUartGpio
{
    sel_0 = 0,
    sel_1,
    sel_2,
    rx_sel_n
};

class SerialUartMux final :
    public sdbusplus::server::object_t<
        sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::
            HostSelector>,
    public ButtonIface
{
  public:
    SerialUartMux(sdbusplus::bus::bus& bus, const char* path, EventPtr& event,
                  buttonConfig& buttonCfg) :
        sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Chassis::
                                        Buttons::server::HostSelector>(
            bus, path, action::defer_emit),
        ButtonIface(bus, event, buttonCfg)
    {
        init();
        // read the platform specific config of host number to uart mux map
        serialUartMuxMap = buttonCfg.extraJsonInfo.at("serial_uart_mux_map")
                               .get<std::map<std::string, int>>();
        debugCardPresentGpio = buttonCfg.extraJsonInfo["debug_present_gpio"];
        maxPosition(buttonCfg.extraJsonInfo["max_position"], true);
        gpioLineCount = buttonCfg.gpios.size();
    }

    ~SerialUartMux()
    {
        deInit();
    }
    void init() override;
    static const std::string_view getFormFactorName()
    {
        return SERIAL_CONSOLE_SWITCH;
    }
    static const char* getDbusObjectPath()
    {
        return SERIAL_CONSOLE_MUX_DBUS_OBJECT_NAME;
    }
    void hostSelectorPositionChanged(sdbusplus::message::message& msg);
    void configSerialConsoleMux(size_t position);
    bool isOCPDbgCardPresent();

    void handleEvent(sd_event_source*, int, uint32_t)
    {}

  protected:
    size_t gpioLineCount;
    uint8_t debugCardPresentGpio;
    std::unique_ptr<sdbusplus::bus::match_t> hsPosChanged;
    std::map<std::string, int> serialUartMuxMap;
};

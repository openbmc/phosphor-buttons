
#pragma once
#include "button_factory.hpp"
#include "button_interface.hpp"
#include "common.hpp"
#include "config.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/HostSelector/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"
#include "xyz/openbmc_project/Inventory/Item/server.hpp"

#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
static constexpr std::string_view DEBUG_CARD_PRESENT_GPIO =
    "debug_card_present";
static constexpr std::string_view SERIAL_CONSOLE_SWITCH = "SERIAL_UART_MUX";

class SerialUartMux final : public ButtonIface
{
  public:
    SerialUartMux(sdbusplus::bus_t& bus, [[maybe_unused]] const char* path,
                  EventPtr& event, buttonConfig& buttonCfg) :
        ButtonIface(bus, event, buttonCfg)
    {
        init();

        // read the platform specific config of host number to uart mux map
        std::unordered_map<std::string, size_t> uartMuxMapJson =
            buttonCfg.extraJsonInfo.at("serial_uart_mux_map")
                .get<decltype(uartMuxMapJson)>();
        for (auto& [key, value] : uartMuxMapJson)
        {
            auto index = std::stoi(key);
            serialUartMuxMap[index] = value;
        }
        if (buttonCfg.gpios.size() < 3)
        {
            throw std::runtime_error("not enough gpio configs found");
        }

        for (auto& gpio : buttonCfg.gpios)
        {
            if (gpio.name == DEBUG_CARD_PRESENT_GPIO)
            {
                debugCardPresentGpio = gpio;
                break;
            }
        }

        gpioLineCount = buttonCfg.gpios.size() - 1;
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
        return "NO_DBUS_OBJECT";
    }

    void hostSelectorPositionChanged(sdbusplus::message_t& msg);
    void configSerialConsoleMux(size_t position);
    bool isOCPDebugCardPresent();

    void handleEvent(sd_event_source*, int, uint32_t) {}

  protected:
    size_t gpioLineCount;
    std::unique_ptr<sdbusplus::bus::match_t> hostPositionChanged;
    gpioInfo debugCardPresentGpio;
    std::unordered_map<size_t, size_t> serialUartMuxMap;
};

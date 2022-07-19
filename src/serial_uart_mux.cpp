#include "serial_uart_mux.hpp"

#include <error.h>

#include <phosphor-logging/lg2.hpp>

namespace sdbusRule = sdbusplus::bus::match::rules;
// add the button iface class to registry
static ButtonIFRegister<SerialUartMux> buttonRegister;

constexpr std::string_view DEBUG_CARD_PRESENT_GPIO = "debug_card_present";
constexpr std::string_view SERIAL_UART_RX_GPIO = "serial_uart_rx";

void SerialUartMux::init()
{
    try
    {
        // when Host Selector Position is changed call the handler

        hostPositionChanged = std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusRule::propertiesChanged(sdbusRule::path(HS_DBUS_OBJECT_NAME),
                                         ""),
            std::bind(std::mem_fn(&SerialUartMux::hostSelectorPositionChanged),
                      this, std::placeholders::_1));
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed binding to matching function : {BUTTON_TYPE}",
                   "BUTTON_TYPE", getFormFactorName());
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
}
// check the debug card present pin
bool SerialUartMux::isOCPDebugCardPresent()
{
    gpioInfo debugCardPresentGpio;
    for (auto& gpio : config.gpios)
    {
        if (gpio.name == DEBUG_CARD_PRESENT_GPIO)
        {
            debugCardPresentGpio = gpio;
            break;
        }
    }
    auto gpioState =
        getGpioState(debugCardPresentGpio.fd, debugCardPresentGpio.polarity);
    return (gpioState == GpioState::high);
}
// set the serial uart MUX to select the console w.r.t host selector position
void SerialUartMux::configSerialConsoleMux(size_t position)
{
    auto debugCardpresent = false;

    auto setUartMuxGpio = [this, &debugCardpresent, &position](int gpioNum) {
        auto gpioState = GpioState::invalid;
        gpioInfo gpioConfig = config.gpios[gpioNum];
        std::string gpioName = gpioConfig.name;
        GpioPolarity polarity = gpioConfig.polarity;

        if (gpioName == SERIAL_UART_RX_GPIO)
        {
            gpioState = debugCardpresent ? GpioState::high : GpioState::low;
        }
        else if (serialUartMuxMap[position] & (0x1 << gpioNum))
        {
            gpioState = GpioState::high;
        }
        else
        {
            gpioState = GpioState::low;
        }
        setGpioState(gpioConfig.fd, polarity, gpioState);
    };

    debugCardpresent = isOCPDebugCardPresent();

    if (debugCardpresent)
    {
        lg2::info("Debug card is present ");
    }
    else
    {
        lg2::info("Debug card not present ");
    }

    for (size_t uartMuxSel = 0; uartMuxSel < gpioLineCount; uartMuxSel++)
    {
        setUartMuxGpio(uartMuxSel);
    }
}

void SerialUartMux::hostSelectorPositionChanged(
    sdbusplus::message::message& msg)
{
    std::string Interface;
    std::string propertyName;
    std::map<std::string, std::variant<size_t>> propertiesChanged;
    lg2::info("hostSelectorPositionChanged callback : {BUTTON_TYPE}",
              "BUTTON_TYPE", getFormFactorName());

    try
    {
        msg.read(Interface, propertiesChanged);
        for (auto& property : propertiesChanged)
        {
            propertyName = property.first;
            if (propertyName == "Position")
            {
                auto hostPosition = std::get<size_t>(property.second);
                lg2::debug("property changed event : {VALUE}", "VALUE",
                           hostPosition);
                configSerialConsoleMux(hostPosition);
                return;
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("exception while reading dbus property : {EXCEPTION}",
                   "EXCEPTION", e.what());
        return;
    }
}

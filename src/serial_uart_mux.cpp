#include "serial_uart_mux.hpp"

#include "xyz/openbmc_project/Chassis/Buttons/HostSelector/server.hpp"

#include <error.h>

#include <phosphor-logging/lg2.hpp>
namespace sdbusRule = sdbusplus::bus::match::rules;
// add the button iface class to registry
static ButtonIFRegister<SerialUartMux> buttonRegister;
namespace HostSelectorServerObj =
    sdbusplus::xyz::openbmc_project::Chassis::Buttons::server;

constexpr std::string_view SERIAL_UART_RX_GPIO = "serial_uart_rx";

void SerialUartMux::init()
{
    try
    {
        // when Host Selector Position is changed call the handler

        hostPositionChanged = std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusRule::type::signal() + sdbusRule::member("PropertiesChanged") +
                sdbusRule::path(HS_DBUS_OBJECT_NAME) +
                sdbusRule::interface("org.freedesktop.DBus.Properties"),
            std::bind(std::mem_fn(&SerialUartMux::hostSelectorPositionChanged),
                      this, std::placeholders::_1));
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed binding to matching function : {BUTTON_TYPE},Exception : {ERROR}",
            "BUTTON_TYPE", getFormFactorName(), "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
}
// check the debug card present pin
bool SerialUartMux::isOCPDebugCardPresent()
{
    auto gpioState =
        getGpioState(debugCardPresentGpio.fd, debugCardPresentGpio.polarity);
    return (gpioState == GpioState::high);
}
// set the serial uart MUX to select the console w.r.t host selector position
void SerialUartMux::configSerialConsoleMux(size_t position)
{
    auto debugCardpresent = false;

    auto setUartMuxGpio = [this, debugCardpresent, position](int gpioNum) {
        auto gpioState = GpioState::invalid;
        const gpioInfo& gpioConfig = config.gpios[gpioNum];

        if (gpioConfig.name == SERIAL_UART_RX_GPIO)
        {
            gpioState = debugCardpresent ? GpioState::high : GpioState::low;
        }
        else
        {
            gpioState = (serialUartMuxMap[position] & (0x1 << gpioNum))
                            ? GpioState::high
                            : GpioState::low;
        }
        setGpioState(gpioConfig.fd, gpioConfig.polarity, gpioState);
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

void SerialUartMux::hostSelectorPositionChanged(sdbusplus::message_t& msg)
{
    std::string interface;
    std::map<std::string,
             HostSelectorServerObj::HostSelector::PropertiesVariant>
        propertiesChanged;
    lg2::info("hostSelectorPositionChanged callback : {BUTTON_TYPE}",
              "BUTTON_TYPE", getFormFactorName());

    try
    {
        msg.read(interface, propertiesChanged);
        for (auto& property : propertiesChanged)
        {
            auto propertyName = property.first;
            if (propertyName == "Position")
            {
                size_t hostPosition = std::get<size_t>(property.second);
                lg2::debug("property changed : {VALUE}", "VALUE", hostPosition);
                configSerialConsoleMux(hostPosition);
                return;
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("exception while reading dbus property : {ERROR}", "ERROR",
                   e.what());
        return;
    }
}

#include "serial_uart_mux.hpp"

#include <error.h>

namespace sdbusRule = sdbusplus::bus::match::rules;
// add the button iface class to registry
static ButtonIFRegister<SerialUartMux> buttonRegister;

void SerialUartMux::init()
{
    try
    {
        // when Host Selector Position is changed call the handler

        hsPosChanged = std::make_unique<sdbusplus::bus::match_t>(
            bus,
            sdbusRule::type::signal() + sdbusRule::member("PropertiesChanged") +
                sdbusRule::path(HS_DBUS_OBJECT_NAME) +
                sdbusRule::interface("org.freedesktop.DBus.Properties"),
            std::bind(std::mem_fn(&SerialUartMux::hostSelectorPositionChanged),
                      this, std::placeholders::_1));
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to add match function : {BUTTON_TYPE}",
                   "BUTTON_TYPE", getFormFactorName());
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
}
// check the debug card present pin
bool SerialUartMux::isOCPDbgCardPresent()
{
    int result = -1;
    char buf = '0';
    int fd = config.gpios[debugCardPresentGpio].fd;
    result = ::lseek(fd, 0, SEEK_SET);

    if (result < 0)
    {
        lg2::error("{TYPE}: debug card present Gpio fd lseek error: {ERROR}",
                   "TYPE", getFormFactorType(), "ERROR", errno);
        return false;
    }

    result = ::read(fd, &buf, sizeof(buf));
    if (result < 0)
    {
        lg2::error(
            "{TYPE}: debug card present Gpio present fd read error: {ERROR}",
            "TYPE", getFormFactorType(), "ERROR", errno);
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    // read the gpio state for the io event received
    GpioState gpioState = (buf == '1') ? (GpioState::low) : (GpioState::high);

    return (gpioState == GpioState::low);
}
// set the serial uart MUX to select the console w.r.t host selector position
void SerialUartMux::configSerialConsoleMux(size_t position)
{
    auto debugCardpresent = false;

    auto setUartMuxGpio = [this, &debugCardpresent,
                           &position](SerialUartGpio gpioNum) {
        char writeBuffer = '0';
        if (gpioNum == SerialUartGpio::rx_sel_n)
        {
            writeBuffer = debugCardpresent ? '0' : '1';
        }
        else
        {
            writeBuffer = (serialUartMuxMap[std::to_string(position)] &
                           (0x1 << static_cast<uint8_t>(gpioNum)))
                              ? '1'
                              : '0';
        }

        lg2::info(" write buf value for {UARTGPIONUM} : {WRITEBUF}: ",
                  "WRITEBUF", static_cast<char>(writeBuffer), "UARTGPIONUM",
                  static_cast<uint8_t>(gpioNum));

        auto result = ::write(config.gpios[gpioNum].fd, &writeBuffer,
                              sizeof(writeBuffer));
        if (result < 0)
        {
            lg2::error("{TYPE}:  Gpio fd write error: {ERROR}", "TYPE",
                       getFormFactorType(), "ERROR", errno);
        }
    };

    debugCardpresent = isOCPDbgCardPresent();

    if (debugCardpresent)
    {
        lg2::info("Debug card is present ");
    }
    else
    {
        lg2::info("Debug card not present ");
    }
    setUartMuxGpio(SerialUartGpio::sel_0);
    setUartMuxGpio(SerialUartGpio::sel_1);
    setUartMuxGpio(SerialUartGpio::sel_2);
    setUartMuxGpio(SerialUartGpio::rx_sel_n);
}

void SerialUartMux::hostSelectorPositionChanged(
    sdbusplus::message::message& msg)
{
    std::string thresholdInterface;
    std::string event;
    std::map<std::string, std::variant<size_t>> propertiesChanged;
    lg2::error("hostSelectorPositionChanged callback : {BUTTON_TYPE}",
               "BUTTON_TYPE", getFormFactorName());

    try
    {
        msg.read(thresholdInterface, propertiesChanged);
        if (propertiesChanged.empty())
        {
            return;
        }

        event = propertiesChanged.begin()->first;
        if (event.empty() || event != "Position")
        {
            return;
        }

        auto hsPosValue = std::get<size_t>(propertiesChanged.begin()->second);

        lg2::debug("property changed event : {VALUE}", "VALUE", hsPosValue);
        position(hsPosValue, true);
        configSerialConsoleMux(hsPosValue);
    }
    catch (const std::exception& e)
    {
        lg2::error("exception while reading dbus property ");
        return;
    }
}

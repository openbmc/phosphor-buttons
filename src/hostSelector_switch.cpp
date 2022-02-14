
#include "hostSelector_switch.hpp"

// add the button iface class to registry
static ButtonIFRegister<HostSelector> buttonRegister;
using namespace phosphor::logging;
size_t HostSelector::getMappedHSConfig(size_t hsPosition)
{
    size_t adjustedPosition = INVALID_INDEX; // set bmc as default value
    std::string hsPosStr;
    hsPosStr = std::to_string(hsPosition);

    if (hsPosMap.find(hsPosStr) != hsPosMap.end())
    {
        adjustedPosition = hsPosMap[hsPosStr];
    }
    else
    {
        log<level::DEBUG>(
            "getMappedHSConfig : no valid value in map.",
            entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
    }
    return adjustedPosition;
}

size_t HostSelector::getGpioIndex(int fd)
{
    for (size_t index = 0; index < 4; index++)
    {
        if (config.gpios[index].fd == fd)
        {
            return index;
        }
    }
    return INVALID_INDEX;
}
void HostSelector::setInitialHostSelectorValue()
{
    char buf;
    for (int index = 0; index < 4; index++)
    {
        auto result = ::lseek(config.gpios[index].fd, 0, SEEK_SET);

        if (result < 0)
        {
            log<level::ERR>(
                "gpio fd lseek error!",
                entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        result = ::read(config.gpios[index].fd, &buf, sizeof(buf));
        if (result < 0)
        {
            log<level::ERR>(
                "gpio fd read error!",
                entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }
        GpioState gpioState =
            (buf == '0') ? (GpioState::low) : (GpioState::high);
        setHostSelectorValue(config.gpios[index].fd, gpioState);
        size_t hsPosMapped = getMappedHSConfig(hostSelectorPosition);
        if (hsPosMapped != INVALID_INDEX)
        {
            position(hsPosMapped, true);
        }
    }
}

void HostSelector::setHostSelectorValue(int fd, GpioState state)
{
    size_t pos = getGpioIndex(fd);

    if (pos == INVALID_INDEX)
    {
        return;
    }
    auto set_bit = [](size_t& val, size_t n) { val |= 0xff & (1 << n); };

    auto clr_bit = [](size_t& val, size_t n) { val &= ~(0xff & (1 << n)); };

    auto bit_op = (state == GpioState::low) ? set_bit : clr_bit;

    bit_op(hostSelectorPosition, pos);
    return;
}
/**
 * @brief This method is called from sd-event provided callback function
 * callbackHandler if platform specific event handling is needed then a
 * derived class instance with its specific event handling logic along with
 * init() function can be created to override the default event handling
 */

void HostSelector::handleEvent(sd_event_source* es, int fd, uint32_t revents)
{
    int n = -1;
    char buf = '0';

    n = ::lseek(fd, 0, SEEK_SET);

    if (n < 0)
    {
        log<level::ERR>(
            "gpio fd lseek error!",
            entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
        return;
    }

    n = ::read(fd, &buf, sizeof(buf));
    if (n < 0)
    {
        log<level::ERR>(
            "gpio fd read error!",
            entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    // read the gpio state for the io event received
    GpioState gpioState = (buf == '0') ? (GpioState::low) : (GpioState::high);

    setHostSelectorValue(fd, gpioState);

    size_t hsPosMapped = getMappedHSConfig(hostSelectorPosition);

    if (hsPosMapped != INVALID_INDEX)
    {
        position(hsPosMapped);
    }
}
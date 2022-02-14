
#include "hostSelector_switch.hpp"

// add the button iface class to registry
static ButtonIFRegister<HostSelector> buttonRegister;

size_t HostSelector::getMappedHSConfig(size_t hsPosition)
{
    size_t adjustedPosition;
    std::string hsPosStr;
    hsPosStr = std::to_string(hsPosition);
    if (hsPosMap.contains(hsPosStr.c_str()))
    {
        adjustedPosition = hsPosMap[hsPosStr.c_str()];
    }
    return adjustedPosition;
}

int HostSelector::getGpioIndex(int fd)
{
    for (int index = 0; index < 4; index++)
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
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        result = ::read(config.gpios[index].fd, &buf, sizeof(buf));
        if (result < 0)
        {
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }

        setHostSelectorValue(config.gpios[index].fd, (buf == '0'));

        std::variant<size_t> hsPositionVariant(
            getMappedHSConfig(hostSelectorPosition));

        setPropertyByName("Position", hsPositionVariant, false);
    }
}

bool HostSelector::setHostSelectorValue(int fd, bool gpioState)
{
    int pos = getGpioIndex(fd);

    if (pos == INVALID_INDEX)
    {
        return false;
    }

    if (gpioState)
    {
        hostSelectorPosition |= 0xf & (0x1 << pos);
    }
    else
    {
        hostSelectorPosition &= ~(0xf & (0x1 << pos));
    }

    return true;
}
/**
 * @brief This method is called from sd-event provided callback function
 * callbackHandler if platform specific event handling is needed then a
 * derived class instance with its specific evend handling logic along with
 * init() function can be created to override the default event handling
 */

void HostSelector::handleEvent(sd_event_source* es, int fd, uint32_t revents)
{
    int n = -1;
    char buf = '0';

    n = ::lseek(fd, 0, SEEK_SET);

    if (n < 0)
    {
        log<level::ERR>((getFormFactorType() + "lseek error!").c_str());
        return;
    }

    n = ::read(fd, &buf, sizeof(buf));
    if (n < 0)
    {
        log<level::ERR>((getFormFactorType() + "read error!").c_str());
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    // read the gpio state for the io event received
    bool gpioState = (buf == '0');
    std::string debugMsg;
    debugMsg = getFormFactorType() +
               "gpio event for fd : " + std::to_string(fd) + " state - " +
               (gpioState ? "1" : "0");
    log<level::DEBUG>(debugMsg.c_str());

    setHostSelectorValue(fd, gpioState);

    std::variant<size_t> hsPositionVariant(
        getMappedHSConfig(hostSelectorPosition));

    setPropertyByName("Position", hsPositionVariant, false);
}
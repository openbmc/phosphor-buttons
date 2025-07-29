
#include "hostSelector_switch.hpp"

#include <error.h>

#include <phosphor-logging/lg2.hpp>

#include <systemd/sd-event.h>
#include "gpio.hpp"

int HostSelector::pollTimerHandler(sd_event_source* eventSource,
                                   uint64_t usec,
                                   void* userData)
{
    auto* hostSelector = static_cast<HostSelector*>(userData);

    for (const auto& gpioInfo : hostSelector->config.gpios)
    {
        // Read raw '0'/'1' and convert to logical GpioState based on polarity
        GpioState state = getGpioState(gpioInfo.fd, gpioInfo.polarity);

        // Update the internal bitmask: deassert → set bit, assert → clear bit
        hostSelector->setHostSelectorValue(gpioInfo.fd, state);

        lg2::debug("GPIO {NUM} state is {STATE}",
                   "NUM",   gpioInfo.number,
                   "STATE", state);
    }


    size_t mappedIndex =
        hostSelector->getMappedHSConfig(hostSelector->hostSelectorPosition);
    if (mappedIndex != INVALID_INDEX)
    {
        hostSelector->position(mappedIndex);
    }

    int intervalMs =
        hostSelector->config.extraJsonInfo.value("polling_interval_ms", 2000);
    uint64_t nextUsec = usec + uint64_t(intervalMs) * 1000;

    sd_event_source_set_time(eventSource, nextUsec);
    sd_event_source_set_enabled(eventSource, SD_EVENT_ON);

    return 0;
}
// add the button iface class to registry
static ButtonIFRegister<HostSelector> buttonRegister;

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
        lg2::debug("getMappedHSConfig : {TYPE}: no valid value in map.", "TYPE",
                   getFormFactorType());
    }
    return adjustedPosition;
}

size_t HostSelector::getGpioIndex(int fd)
{
    for (size_t index = 0; index < gpioLineCount; index++)
    {
        if (config.gpios[index].fd == fd)
        {
            return index;
        }
    }
    return INVALID_INDEX;
}

char HostSelector::getValueFromFd(int fd)
{
    char buf;
    auto result = ::lseek(fd, 0, SEEK_SET);

    if (result < 0)
    {
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }

    result = ::read(fd, &buf, sizeof(buf));
    if (result < 0)
    {
        throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
            IOError();
    }
    return buf;
}

void HostSelector::setInitialHostSelectorValue()
{
    size_t hsPosMapped = 0;

    try
    {
        if (config.type == ConfigType::gpio)
        {
            for (size_t index = 0; index < gpioLineCount; index++)
            {
                GpioState gpioState =
                    (getValueFromFd(config.gpios[index].fd) == '0')
                        ? (GpioState::deassert)
                        : (GpioState::assert);
                setHostSelectorValue(config.gpios[index].fd, gpioState);
            }
            hsPosMapped = getMappedHSConfig(hostSelectorPosition);
        }
        else if (config.type == ConfigType::cpld)
        {
            hsPosMapped = getValueFromFd(config.cpld.cpldMappedFd) - '0';
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("{TYPE}: exception while reading fd : {ERROR}", "TYPE",
                   getFormFactorType(), "ERROR", e.what());
    }

    if (config.extraJsonInfo.value("polling_mode", false))
    {
        // If polling mode is enabled, set up a timer to poll the GPIO state
        int intervalMs = config.extraJsonInfo.value("polling_interval_ms", 50);
        sd_event_source* timerSource = nullptr;
        int rc = sd_event_add_time(
            event.get(),
            &timerSource,
            CLOCK_MONOTONIC,
            0,
            (uint64_t)intervalMs * 1000,
            pollTimerHandler,
            this
        );
        if (rc < 0)
        {
            lg2::error("Failed to start poll timer: {ERR}", "ERR", rc);
        }
        else
        {
            lg2::info("Started polling mode: {MS}ms", "MS", intervalMs);
        }
    }

    if (hsPosMapped != INVALID_INDEX)
    {
        position(hsPosMapped, true);
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

    auto bit_op = (state == GpioState::deassert) ? set_bit : clr_bit;

    bit_op(hostSelectorPosition, pos);
    return;
}
/**
 * @brief This method is called from sd-event provided callback function
 * callbackHandler if platform specific event handling is needed then a
 * derived class instance with its specific event handling logic along with
 * init() function can be created to override the default event handling
 */

void HostSelector::handleEvent(sd_event_source* /* es */, int fd,
                               uint32_t /* revents */)
{
    char buf = '0';
    try
    {
        buf = getValueFromFd(fd);
    }
    catch (const std::exception& e)
    {
        lg2::error("{TYPE}: exception while reading fd : {ERROR}", "TYPE",
                   getFormFactorType(), "ERROR", e.what());
        return;
    }

    size_t hsPosMapped = 0;
    if (config.type == ConfigType::gpio)
    {
        // read the gpio state for the io event received
        GpioState gpioState =
            (buf == '0') ? (GpioState::deassert) : (GpioState::assert);

        setHostSelectorValue(fd, gpioState);
        hsPosMapped = getMappedHSConfig(hostSelectorPosition);
    }
    else if (config.type == ConfigType::cpld)
    {
        hsPosMapped = buf - '0';
    }

    if (hsPosMapped != INVALID_INDEX)
    {
        position(hsPosMapped);
    }
}

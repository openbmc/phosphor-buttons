#include "debugHostSelector_button.hpp"
// add the button iface class to registry
static ButtonIFRegister<DebugHostSelector> buttonRegister;
using namespace phosphor::logging;

void DebugHostSelector::simPress()
{
    pressed();
}

void DebugHostSelector::simRelease()
{
    released();
}

/**
 * @brief This method is called from sd-event provided callback function
 * callbackHandler if platform specific event handling is needed then a
 * derived class instance with its specific event handling logic along with
 * init() function can be created to override the default event handling
 */

void DebugHostSelector::handleEvent(sd_event_source* es, int fd,
                                    uint32_t revents)
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

    if (buf == '0')
    {
        log<level::INFO>(
            "Button pressed",
            entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
        // emit pressed signal
        pressed();
    }
    else
    {
        log<level::INFO>(
            "Button released",
            entry("FORM_FACTOR_TYPE=%s", (getFormFactorType()).c_str()));
        // emit released signal
        released();
    }
}
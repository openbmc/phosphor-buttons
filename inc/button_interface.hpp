#pragma once

#include "common.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
// This is the base class for all the button interface types
//
class ButtonIface
{

  public:
    ButtonIface(sdbusplus::bus::bus& bus, EventPtr& event,
                buttonConfig& buttonCfg,
                sd_event_io_handler_t handler = ButtonIface::EventHandler) :
        bus(bus),
        event(event), config(buttonCfg), callbackHandler(handler)
    {
        int ret = -1;

        // config group gpio based on the gpio defs read from the json file
        ret = configGroupGpio(config);

        if (ret < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                (getFormFactorType() + " : failed to config GPIO").c_str());
            throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                IOError();
        }
    }
    virtual ~ButtonIface()
    {
    }

    /**
     * @brief This method is called from sd-event provided callback function
     * callbackHandler if platform specific event handling is needed then a
     * derived class instance with its specific evend handling logic along with
     * init() function can be created to override the default event handling.
     */

    virtual void handleEvent(sd_event_source* es, int fd, uint32_t revents) = 0;
    static int EventHandler(sd_event_source* es, int fd, uint32_t revents,
                            void* userdata)
    {
        if (userdata)
        {

            ButtonIface* buttonIface = static_cast<ButtonIface*>(userdata);
            buttonIface->handleEvent(es, fd, revents);
        }

        return 0;
    }

    std::string getFormFactorType() const
    {
        return config.formFactorName;
    }

  protected:
    /**
     * @brief oem specific initialization can be done under init function.
     * if platform specific initialization is needed then
     * a derived class instance with its own init function to override the
     * default init() method can be added.
     */

    virtual void init()
    {

        // initialize the button io fd from the buttonConfig
        // which has fd stored when configGroupGpio is called
        for (auto gpioCfg : config.gpios)
        {
            char buf;
            int fd = gpioCfg.fd;

            int ret = ::read(fd, &buf, sizeof(buf));
            if (ret < 0)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    (getFormFactorType() + " : read error!").c_str());
            }

            ret = sd_event_add_io(event.get(), nullptr, fd, EPOLLPRI,
                                  callbackHandler, this);
            if (ret < 0)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    (getFormFactorType() + " : failed to add to event loop")
                        .c_str());
                ::closeGpio(fd);
                throw sdbusplus::xyz::openbmc_project::Chassis::Common::Error::
                    IOError();
            }
        }
    }
    /**
     * @brief similar to init() oem specific deinitialization can be done under
     * deInit function. if platform specific deinitialization is needed then a
     * derived class instance with its own init function to override the default
     * deinit() method can be added.
     */
    virtual void deInit()
    {
        for (auto gpioCfg : config.gpios)
        {
            ::closeGpio(gpioCfg.fd);
        }
    }

    buttonConfig config;
    sdbusplus::bus::bus& bus;
    EventPtr& event;
    sd_event_io_handler_t callbackHandler;
};

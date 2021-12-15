#pragma once

#include "common.hpp"
#include "gpio.hpp"

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
    }
    /**
     * @brief oem specific initialization can be done under init function.
     * if platform specific initialization is needed then
     * a derived class instance with its own init function to override the
     * default init() method can be added.
     */

    virtual void init() = 0;
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
            return 0;
        }
    }

    virtual std::string getFormFactorType()
    {
        return config.formFactorName;
    }

  protected:
    buttonConfig config;
    sdbusplus::bus::bus& bus;
    EventPtr& event;
    sd_event_io_handler_t callbackHandler;
};

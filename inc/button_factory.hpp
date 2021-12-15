#pragma once

#include "button_interface.hpp"
#include "gpio.hpp"

#include <map>
#include <phosphor-logging/elog-errors.hpp>

// This map is the registry for keeping supported button interface types.
inline std::map<std::string, std::function<std::unique_ptr<buttonIface>(
                                 sdbusplus::bus::bus& bus, EventPtr& event,
                                 buttonConfig& buttonCfg)>>
    buttonIfaceRegistry;

/**
 * @brief This is abstract factory for the creating phosphor buttons objects
 * based on the button  / formfactor type given.
 */

class buttonFactory
{
  public:
    /**
     * @brief this method creates a key and value pair element
     * for the given button interface where key is the form factor
     * name and the value is lambda method to return
     * the instance of the button interface.
     * This key value pair is stored in the Map buttonIfaceRegistry.
     */

    template <typename T>
    static void addToRegistry()
    {

        buttonIfaceRegistry[T::template getFormFactorName<T>()] =
            [](sdbusplus::bus::bus& bus, EventPtr& event,
               buttonConfig& buttonCfg) {
                return std::make_unique<T>(bus, T::getDbusObjectPath(), event,
                                           buttonCfg);
            };
    }
    /**
     * @brief this method returns the button interface object
     *    corresponding to the button formfactor name provided
     */
    static std::unique_ptr<buttonIface> createInstance(std::string name,
                                                       sdbusplus::bus::bus& bus,
                                                       EventPtr& event,
                                                       buttonConfig& buttonCfg)
    {
        std::unique_ptr<buttonIface> instance;

        // find matching name in the registry and call factory method.
        auto it = buttonIfaceRegistry.find(name);
        if (it != buttonIfaceRegistry.end())
            instance = it->second(bus, event, buttonCfg);

        if (instance != nullptr)
            return std::move(instance);
        else
            return nullptr;
    }
};

template <class T>
class buttonIFRegister
{
  public:
    buttonIFRegister()
    {
        // register the class factory function
        buttonFactory::addToRegistry<T>();
    }
};
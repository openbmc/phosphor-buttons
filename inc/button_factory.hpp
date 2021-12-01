#pragma once

#include "button_interface.hpp"
#include "gpio.hpp"
#include "hostSelector_switch.hpp"
#include "id_button.hpp"
#include "power_button.hpp"
#include "reset_button.hpp"

#include <map>

// This map is the registry for keeping supported button interface types.
inline std::map<std::string, std::function<std::unique_ptr<buttonIface>(
                                 sdbusplus::bus::bus& bus, EventPtr& event,
                                 buttonConfig& buttonCfg)>>
    factoryMethodRegistry;

/**
 * @brief This is abstract factory for the creating phosphor buttons objects
 * based on the button  / formfactor type given.
 */

class buttonFactory
{

  public:
    buttonFactory()
    {
        // list of supported phosphor button interfaces added to the registry
        addToRegistry<PowerButton>();
        addToRegistry<ResetButton>();
        addToRegistry<IDButton>();
        addToRegistry<HostSelector>();
    }
    template <typename T>
    void addToRegistry()
    {

        factoryMethodRegistry[T::template getFormFactorName<T>()] =
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
        auto it = factoryMethodRegistry.find(name);
        if (it != factoryMethodRegistry.end())
            instance = it->second(bus, event, buttonCfg);

        if (instance != nullptr)
            return std::move(instance);
        else
            return nullptr;
    }
};

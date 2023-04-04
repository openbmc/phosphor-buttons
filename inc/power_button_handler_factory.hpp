#pragma once

#include "config.h"

#include "power_button_handler.hpp"

#include <memory>
#include <unordered_map>

namespace phosphor::button
{

using powerButtonHandlerCreator =
    std::function<std::unique_ptr<PowerButtonHandler>(sdbusplus::bus_t& bus)>;

/**
 * @class PowerButtonHandlerFactory
 *
 * Creates the custom power button handler class if one is set with
 * the 'power-button-handler' meson option.
 *
 * The createHandler() method will return a nullptr if no custom
 * handler is enabled.
 */
class PowerButtonHandlerFactory
{
  public:
    static PowerButtonHandlerFactory& instance()
    {
        static PowerButtonHandlerFactory factory;
        return factory;
    }

    template <typename T>
    void addToRegistry()
    {
        handlerRegistry[std::string(T::getName())] = [](sdbusplus::bus_t& bus) {
            return std::make_unique<T>(bus);
        };
    }

    std::unique_ptr<PowerButtonHandler> createHandler(sdbusplus::bus_t& bus)
    {
        // Find the creator method named after the
        // 'power-button-handler' option value.
        auto objectIter = handlerRegistry.find(POWER_BUTTON_HANDLER);
        if (objectIter != handlerRegistry.end())
        {
            return objectIter->second(bus);
        }
        else
        {
            return nullptr;
        }
    }

  private:
    PowerButtonHandlerFactory() = default;

    std::unordered_map<std::string, powerButtonHandlerCreator> handlerRegistry;
};

/**
 * @brief Registers a power button handler with the factory.
 *
 * Declare a static instance of this at the top of the handler
 * .cpp file like:
 *    static PowerButtonHandlerRegister<MyClass> register;
 */
template <class T>
class PowerButtonHandlerRegister
{
  public:
    PowerButtonHandlerRegister()
    {
        PowerButtonHandlerFactory::instance().addToRegistry<T>();
    }
};
} // namespace phosphor::button

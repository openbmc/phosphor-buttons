#include "config.h"

#include "button_handler.hpp"

#include <phosphor-logging/lg2.hpp>
#include <iostream>
#include <string>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>
namespace phosphor
{
namespace button
{

namespace sdbusRule = sdbusplus::bus::match::rules;
using namespace sdbusplus::xyz::openbmc_project::State::server;

constexpr auto chassisIface = "xyz.openbmc_project.State.Chassis";
constexpr auto hostIface = "xyz.openbmc_project.State.Host";
constexpr auto powerButtonIface = "xyz.openbmc_project.Chassis.Buttons.Power";
constexpr auto idButtonIface = "xyz.openbmc_project.Chassis.Buttons.ID";
constexpr auto resetButtonIface = "xyz.openbmc_project.Chassis.Buttons.Reset";
constexpr auto ledGroupIface = "xyz.openbmc_project.Led.Group";
constexpr auto ledGroupBasePath = "/xyz/openbmc_project/led/groups/";
constexpr auto hostSelectorIface =
    "xyz.openbmc_project.Chassis.Buttons.HostSelector";
constexpr auto debugHostSelectorIface =
    "xyz.openbmc_project.Chassis.Buttons.Button";

constexpr auto propertyIface = "org.freedesktop.DBus.Properties";
constexpr auto mapperIface = "xyz.openbmc_project.ObjectMapper";

constexpr auto mapperObjPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto BMC_POSITION = 0;

Handler::Handler(sdbusplus::bus_t& bus) : bus(bus)
{
    try
    {
        if (!getService(POWER_DBUS_OBJECT_NAME, powerButtonIface).empty())
        {
            lg2::info("Starting power button handler");
            powerButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(POWER_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(powerButtonIface),
                std::bind(std::mem_fn(&Handler::powerReleased), this,
                          std::placeholders::_1));

            powerButtonLongPressed = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("PressedLong") +
                    sdbusRule::path(POWER_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(powerButtonIface),
                std::bind(std::mem_fn(&Handler::longPowerPressed), this,
                          std::placeholders::_1));
        }

        if (getService(HS_DBUS_OBJECT_NAME, hostSelectorIface).empty())
        {
            int chassisMax = numberOfChassis();
            for (int chassisNum = 0; chassisNum <= chassisMax; chassisNum++)
            {
                lg2::info("Starting multi power button handler");
                if (!getService(POWER_DBUS_OBJECT_NAME + std::to_string(chassisNum), powerButtonIface).empty())
                {
                    std::shared_ptr<sdbusplus::bus::match_t> multiPowerReleaseMatch = std::make_shared<sdbusplus::bus::match_t>(
                        bus,
                        sdbusRule::type::signal() + sdbusRule::member("Released") +
                            sdbusRule::path(POWER_DBUS_OBJECT_NAME + std::to_string(chassisNum)) +
                            sdbusRule::interface(powerButtonIface),
                        std::bind(std::mem_fn(&Handler::powerReleased), this,
                                std::placeholders::_1));
                    multiPowerButtonReleased.emplace_back(multiPowerReleaseMatch);

                    std::shared_ptr<sdbusplus::bus::match_t> multiPowerLongPressedMatch = std::make_shared<sdbusplus::bus::match_t>(
                        bus,
                        sdbusRule::type::signal() + sdbusRule::member("PressedLong") +
                            sdbusRule::path(POWER_DBUS_OBJECT_NAME + std::to_string(chassisNum)) +
                            sdbusRule::interface(powerButtonIface),
                        std::bind(std::mem_fn(&Handler::longPowerPressed), this,
                                std::placeholders::_1));
                    multiPowerButtonLongPressed.emplace_back(multiPowerLongPressedMatch);

                    std::shared_ptr<sdbusplus::bus::match_t> multiPowerLongerPressedMatch = std::make_shared<sdbusplus::bus::match_t>(
                        bus,
                        sdbusRule::type::signal() + sdbusRule::member("PressedLonger") +
                            sdbusRule::path(POWER_DBUS_OBJECT_NAME + std::to_string(chassisNum)) +
                            sdbusRule::interface(powerButtonIface),
                        std::bind(std::mem_fn(&Handler::longerPowerPressed), this,
                                std::placeholders::_1));
                    multiPowerButtonLongerPressed.emplace_back(multiPowerLongerPressedMatch);
                }
            }
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Create power button handler error: {ERROR}",
            "ERROR", e);
    }

    try
    {
        if (!getService(ID_DBUS_OBJECT_NAME, idButtonIface).empty())
        {
            lg2::info("Registering ID button handler");
            idButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(ID_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(idButtonIface),
                std::bind(std::mem_fn(&Handler::idReleased), this,
                          std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        // The button wasn't implemented
    }

    try
    {
        if (!getService(RESET_DBUS_OBJECT_NAME, resetButtonIface).empty())
        {
            lg2::info("Registering reset button handler");
            resetButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(RESET_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(resetButtonIface),
                std::bind(std::mem_fn(&Handler::resetReleased), this,
                          std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        // The button wasn't implemented
    }
    try
    {
        if (!getService(DBG_HS_DBUS_OBJECT_NAME, debugHostSelectorIface)
                 .empty())
        {
            lg2::info("Registering debug host selector button handler");
            debugHSButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(DBG_HS_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(debugHostSelectorIface),
                std::bind(std::mem_fn(&Handler::debugHostSelectorReleased),
                          this, std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        // The button wasn't implemented
    }
}
bool Handler::isMultiHost()
{
    if (static_cast<std::string>(INSTANCES) != "0")
    {
        return true;
    }
    else
    {
        return (!getService(HS_DBUS_OBJECT_NAME, hostSelectorIface).empty());
    }
}
std::string Handler::getService(const std::string& path,
                                const std::string& interface) const
{
    auto method = bus.new_method_call(mapperService, mapperObjPath, mapperIface,
                                      "GetObject");
    method.append(path, std::vector{interface});
    try
    {
        auto result = bus.call(method);
        std::map<std::string, std::vector<std::string>> objectData;
        result.read(objectData);
        return objectData.begin()->first;
    }
    catch (const sdbusplus::exception_t& e)
    {
        return std::string();
    }
}
size_t Handler::getHostSelectorValue()
{
    auto HSService = getService(HS_DBUS_OBJECT_NAME, hostSelectorIface);

    if (HSService.empty())
    {
        lg2::info("Host selector dbus object not available");
        throw std::invalid_argument("Host selector dbus object not available");
    }

    try
    {
        auto method = bus.new_method_call(
            HSService.c_str(), HS_DBUS_OBJECT_NAME, propertyIface, "Get");
        method.append(hostSelectorIface, "Position");
        auto result = bus.call(method);

        std::variant<size_t> HSPositionVariant;
        result.read(HSPositionVariant);

        auto position = std::get<size_t>(HSPositionVariant);
        return position;
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Error reading host selector position: {ERROR}", "ERROR", e);
        throw;
    }
}
bool Handler::poweredOn(size_t hostNumber) const
{
    auto chassisObjectName =
        CHASSIS_STATE_OBJECT_NAME + std::to_string(hostNumber);
    auto service = getService(chassisObjectName.c_str(), chassisIface);
    auto method = bus.new_method_call(
        service.c_str(), chassisObjectName.c_str(), propertyIface, "Get");
    method.append(chassisIface, "CurrentPowerState");
    auto result = bus.call(method);

    std::variant<std::string> state;
    result.read(state);

    return Chassis::PowerState::On ==
           Chassis::convertPowerStateFromString(std::get<std::string>(state));
}

void Handler::handlePowerEvent(PowerEvent powerEventType, std::string chassisPath)
{
    std::string objPathName;
    std::string dbusIfaceName;
    std::string transitionName;
    std::variant<Host::Transition, Chassis::Transition> transition;

    size_t hostNumber = 0;
    std::string hostNumStr(1, chassisPath.back());
    auto isMultiHostSystem = isMultiHost();

    if (!getService(HS_DBUS_OBJECT_NAME, hostSelectorIface).empty())
    {
        hostNumber = getHostSelectorValue();
        lg2::info("Multi-host system detected : {POSITION}", "POSITION",
                hostNumber);

        hostNumStr = std::to_string(hostNumber);

        // ignore  power and reset button events if BMC is selected.
        if (isMultiHostSystem && (hostNumber == BMC_POSITION) &&
            (powerEventType != PowerEvent::longPowerReleased))
        {
            lg2::info(
                "handlePowerEvent : BMC selected on multi-host system. ignoring power and reset button events...");
            return;
        }
    }

    switch (powerEventType)
    {
        case PowerEvent::powerReleased:
        {
            if (!getService(HS_DBUS_OBJECT_NAME, hostSelectorIface).empty())
            {
                objPathName = HOST_STATE_OBJECT_NAME + hostNumStr;
                dbusIfaceName = hostIface;
                transitionName = "RequestedHostTransition";

                transition = Host::Transition::On;

                if (poweredOn(hostNumber))
                {
                    transition = Host::Transition::Off;
                }
                lg2::info("handlePowerEvent : Handle power button press ");
            }
            else
            {
                dbusIfaceName = chassisIface;
                objPathName = CHASSIS_STATE_OBJECT_NAME + hostNumStr;
                transitionName = "RequestedPowerTransition";
                transition = Chassis::Transition::On;
            }
            break;
        }
        case PowerEvent::longPowerReleased:
        {
            dbusIfaceName = chassisIface;
            transitionName = "RequestedPowerTransition";
            objPathName = CHASSIS_STATE_OBJECT_NAME + hostNumStr;
            transition = Chassis::Transition::Off;

            /*  multi host system :
                    hosts (1 to N) - host shutdown
                    bmc (0) - sled cycle
                single host system :
                    host(0) - host shutdown
            */
            if (isMultiHostSystem && (hostNumber == BMC_POSITION))
            {
#if CHASSIS_SYSTEM_RESET_ENABLED
                objPathName = CHASSISSYSTEM_STATE_OBJECT_NAME + hostNumStr;
                transition = Chassis::Transition::PowerCycle;
#else
                return;
#endif
            }
            else if (!poweredOn(hostNumber))
            {
                lg2::info("Power is off so ignoring long power button press");
                return;
            }
            lg2::info("handlePowerEvent : handle long power button press");

            break;
        }
        case PowerEvent::longPowerPressed:
        {
            dbusIfaceName = chassisIface;
            transitionName = "RequestedPowerTransition";
            objPathName = CHASSIS_STATE_OBJECT_NAME + hostNumStr;
            transition = Chassis::Transition::PowerCycle;
            break;
        }
        case PowerEvent::longerPowerPressed:
        {
            dbusIfaceName = chassisIface;
            transitionName = "RequestedPowerTransition";
            objPathName = CHASSIS_STATE_OBJECT_NAME + hostNumStr;
            transition = Chassis::Transition::Off;

            if (hostNumStr == "0")
            {
                objPathName = CHASSIS_STATE_OBJECT_NAME + hostNumStr;
                transition = Chassis::Transition::PowerCycle;
            }
            break;
        }
        case PowerEvent::resetReleased:
        {
            objPathName = HOST_STATE_OBJECT_NAME + hostNumStr;
            dbusIfaceName = hostIface;
            transitionName = "RequestedHostTransition";

            if (!poweredOn(hostNumber))
            {
                lg2::info("Power is off so ignoring reset button press");
                return;
            }

            lg2::info("Handling reset button press");
            transition = Host::Transition::Reboot;
            break;
        }
        default:
        {
            lg2::error("{EVENT} is invalid power event. skipping...", "EVENT",
                       static_cast<std::underlying_type_t<PowerEvent>>(
                           powerEventType));

            return;
        }
    }
    auto service = getService(objPathName.c_str(), dbusIfaceName);
    auto method = bus.new_method_call(service.c_str(), objPathName.c_str(),
                                      propertyIface, "Set");
    method.append(dbusIfaceName, transitionName, transition);
    bus.call(method);
}

void Handler::powerReleased(sdbusplus::message_t& msg/* msg */)
{
    try
    {
        handlePowerEvent(PowerEvent::powerReleased, msg.get_path());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed power state change on a power button press: {ERROR}",
                   "ERROR", e);
    }
}

void Handler::longPowerPressed(sdbusplus::message_t& msg/* msg */)
{
    try
    {
        handlePowerEvent(PowerEvent::longPowerPressed, msg.get_path());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed powering off on long power button press: {ERROR}",
                   "ERROR", e);
    }
}

void Handler::longerPowerPressed(sdbusplus::message_t& msg/* msg */)
{
    try
    {
        handlePowerEvent(PowerEvent::longerPowerPressed, msg.get_path());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed powering off on longer power button press: {ERROR}",
                   "ERROR", e);
    }
}

void Handler::resetReleased(sdbusplus::message_t& msg/* msg */)
{
    try
    {
        handlePowerEvent(PowerEvent::resetReleased, msg.get_path());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed power state change on a reset button press: {ERROR}",
                   "ERROR", e);
    }
}

void Handler::idReleased(sdbusplus::message_t& /* msg */)
{
    std::string groupPath{ledGroupBasePath};
    groupPath += ID_LED_GROUP;

    auto service = getService(groupPath, ledGroupIface);

    if (service.empty())
    {
        lg2::info("No found {GROUP} during ID button press:", "GROUP",
                  groupPath);
        return;
    }

    try
    {
        auto method = bus.new_method_call(service.c_str(), groupPath.c_str(),
                                          propertyIface, "Get");
        method.append(ledGroupIface, "Asserted");
        auto result = bus.call(method);

        std::variant<bool> state;
        result.read(state);

        state = !std::get<bool>(state);

        lg2::info(
            "Changing ID LED group state on ID LED press, GROUP = {GROUP}, STATE = {STATE}",
            "GROUP", groupPath, "STATE", std::get<bool>(state));

        method = bus.new_method_call(service.c_str(), groupPath.c_str(),
                                     propertyIface, "Set");

        method.append(ledGroupIface, "Asserted", state);
        result = bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Error toggling ID LED group on ID button press: {ERROR}",
                   "ERROR", e);
    }
}

void Handler::increaseHostSelectorPosition()
{
    try
    {
        auto HSService = getService(HS_DBUS_OBJECT_NAME, hostSelectorIface);

        if (HSService.empty())
        {
            lg2::error("Host selector service not available");
            return;
        }

        auto method =
            bus.new_method_call(HSService.c_str(), HS_DBUS_OBJECT_NAME,
                                phosphor::button::propertyIface, "GetAll");
        method.append(phosphor::button::hostSelectorIface);
        auto result = bus.call(method);
        std::unordered_map<std::string, std::variant<size_t>> properties;
        result.read(properties);

        auto maxPosition = std::get<size_t>(properties.at("MaxPosition"));
        auto position = std::get<size_t>(properties.at("Position"));

        std::variant<size_t> HSPositionVariant =
            (position < maxPosition) ? (position + 1) : 0;

        method = bus.new_method_call(HSService.c_str(), HS_DBUS_OBJECT_NAME,
                                     phosphor::button::propertyIface, "Set");
        method.append(phosphor::button::hostSelectorIface, "Position");

        method.append(HSPositionVariant);
        result = bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Error modifying host selector position : {ERROR}", "ERROR",
                   e);
    }
}

void Handler::debugHostSelectorReleased(sdbusplus::message_t& /* msg */)
{
    try
    {
        increaseHostSelectorPosition();
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed power process debug host selector button press : {ERROR}",
            "ERROR", e);
    }
}

} // namespace button
} // namespace phosphor

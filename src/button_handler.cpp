#include "config.h"

#include "button_handler.hpp"

#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>
namespace phosphor
{
namespace button
{

namespace sdbusRule = sdbusplus::bus::match::rules;
using namespace sdbusplus::xyz::openbmc_project::State::server;
using namespace phosphor::logging;

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

Handler::Handler(sdbusplus::bus::bus& bus) : bus(bus)
{
    try
    {
        if (!getService(POWER_DBUS_OBJECT_NAME, powerButtonIface).empty())
        {
            log<level::INFO>("Starting power button handler");
            powerButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(POWER_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(powerButtonIface),
                std::bind(std::mem_fn(&Handler::powerReleased), this,
                          std::placeholders::_1));

            powerButtonLongPressReleased =
                std::make_unique<sdbusplus::bus::match_t>(
                    bus,
                    sdbusRule::type::signal() +
                        sdbusRule::member("PressedLong") +
                        sdbusRule::path(POWER_DBUS_OBJECT_NAME) +
                        sdbusRule::interface(powerButtonIface),
                    std::bind(std::mem_fn(&Handler::longPowerReleased), this,
                              std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        // The button wasn't implemented
    }

    try
    {
        if (!getService(ID_DBUS_OBJECT_NAME, idButtonIface).empty())
        {
            log<level::INFO>("Registering ID button handler");
            idButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(ID_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(idButtonIface),
                std::bind(std::mem_fn(&Handler::idReleased), this,
                          std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        // The button wasn't implemented
    }

    try
    {
        if (!getService(RESET_DBUS_OBJECT_NAME, resetButtonIface).empty())
        {
            log<level::INFO>("Registering reset button handler");
            resetButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(RESET_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(resetButtonIface),
                std::bind(std::mem_fn(&Handler::resetReleased), this,
                          std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        // The button wasn't implemented
    }
    try
    {
        if (!getService(DBG_HS_DBUS_OBJECT_NAME, debugHostSelectorIface)
                 .empty())
        {
            log<level::INFO>("Registering debug host selector button handler");
            debugHSButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(DBG_HS_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(debugHostSelectorIface),
                std::bind(std::mem_fn(&Handler::debugHSReleased), this,
                          std::placeholders::_1));
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        // The button wasn't implemented
    }
}
bool Handler::isMultiHost()
{
    // return true in case host selector object is available
    return (!getService(HS_DBUS_OBJECT_NAME, hostSelectorIface).empty());
}
std::string Handler::getService(const std::string& path,
                                const std::string& interface) const
{
    auto method = bus.new_method_call(mapperService, mapperObjPath, mapperIface,
                                      "GetObject");
    method.append(path, std::vector{interface});
    auto result = bus.call(method);

    std::map<std::string, std::vector<std::string>> objectData;
    result.read(objectData);

    return objectData.begin()->first;
}
size_t Handler::getHostSelectorValue()
{
    auto HSService = getService(HS_DBUS_OBJECT_NAME, hostSelectorIface);

    if (HSService.empty())
    {
        log<level::INFO>("Host selector dbus object not available");
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
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Error reading host selector position",
                        entry("ERROR=%s", e.what()));
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

void Handler::handlePowerEvent(PowerEvent powerEventType)
{
    std::string objPathName;
    std::string dbusIfaceName;
    std::string transitionName;
    std::variant<Host::Transition, Chassis::Transition> transition;

    size_t hostNumber = 0;
    auto isMultiHostSystem = isMultiHost();
    if (isMultiHostSystem)
    {
        hostNumber = getHostSelectorValue();
        log<level::INFO>("Multi-host system detected : ",
                         entry("POSITION=%d", hostNumber));
    }

    std::string hostNumStr = std::to_string(hostNumber);

    // ignore  power and reset button events if BMC is selected.
    if (isMultiHostSystem && (hostNumber == BMC_POSITION) &&
        (powerEventType != PowerEvent::longPowerReleased))
    {
        log<level::INFO>("handlePowerEvent : BMC selected on multi-host system."
                         "ignoring power and reset button events...");
        return;
    }

    switch (powerEventType)
    {
        case PowerEvent::powerReleased: {
            objPathName = HOST_STATE_OBJECT_NAME + hostNumStr;
            dbusIfaceName = hostIface;
            transitionName = "RequestedHostTransition";

            transition = Host::Transition::On;

            if (poweredOn(hostNumber))
            {
                transition = Host::Transition::Off;
            }
            log<level::INFO>("handlePowerEvent : Handle power button press ");

            break;
        }
        case PowerEvent::longPowerReleased: {
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
                log<level::INFO>(
                    "Power is off so ignoring long power button press");
                return;
            }
            log<level::INFO>(
                "handlePowerEvent : handle long power button press");

            break;
        }

        case PowerEvent::resetReleased: {

            objPathName = HOST_STATE_OBJECT_NAME + hostNumStr;
            dbusIfaceName = hostIface;
            transitionName = "RequestedHostTransition";

            if (!poweredOn(hostNumber))
            {
                log<level::INFO>("Power is off so ignoring reset button press");
                return;
            }

            log<level::INFO>("Handling reset button press");
            transition = Host::Transition::Reboot;
            break;
        }
        default:
        {
            log<level::ERR>(
                "Invalid power event. skipping...",
                entry("EVENT=%d", static_cast<int>(powerEventType)));

            return;
        }
    }
    auto service = getService(objPathName.c_str(), dbusIfaceName);
    auto method = bus.new_method_call(service.c_str(), objPathName.c_str(),
                                      propertyIface, "Set");
    method.append(dbusIfaceName, transitionName, transition);
    bus.call(method);
}
void Handler::powerReleased(sdbusplus::message::message& msg)
{
    try
    {
        handlePowerEvent(PowerEvent::powerReleased);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed power state change on a power button press",
                        entry("ERROR=%s", e.what()));
    }
}
void Handler::longPowerReleased(sdbusplus::message::message& msg)
{
    try
    {
        handlePowerEvent(PowerEvent::longPowerReleased);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed powering off on long power button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::resetReleased(sdbusplus::message::message& msg)
{
    try
    {
        handlePowerEvent(PowerEvent::resetReleased);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed power state change on a reset button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::idReleased(sdbusplus::message::message& msg)
{
    std::string groupPath{ledGroupBasePath};
    groupPath += ID_LED_GROUP;

    auto service = getService(groupPath, ledGroupIface);

    if (service.empty())
    {
        log<level::INFO>("No identify LED group found during ID button press",
                         entry("GROUP=%s", groupPath.c_str()));
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

        log<level::INFO>("Changing ID LED group state on ID LED press",
                         entry("GROUP=%s", groupPath.c_str()),
                         entry("STATE=%d", std::get<bool>(state)));

        method = bus.new_method_call(service.c_str(), groupPath.c_str(),
                                     propertyIface, "Set");

        method.append(ledGroupIface, "Asserted", state);
        result = bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Error toggling ID LED group on ID button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::increaseHSPosition()
{

    try
    {
        auto HSService = getService(HS_DBUS_OBJECT_NAME, hostSelectorIface);

        if (HSService.empty())
        {
            log<level::ERR>("Host selector service not available");
            return;
        }

        auto getHSProperty = [this](std::string propertyName,
                                    std::string HSDBusName) {
            auto method =
                bus.new_method_call(HSDBusName.c_str(), HS_DBUS_OBJECT_NAME,
                                    phosphor::button::propertyIface, "Get");
            method.append(phosphor::button::hostSelectorIface, propertyName);
            auto result = bus.call(method);
            std::variant<size_t> PropertyVariant;
            result.read(PropertyVariant);
            auto propertyValue = std::get<size_t>(PropertyVariant);

            return propertyValue;
        };

        auto maxPosition = getHSProperty("MaxPosition", HSService);
        auto position = getHSProperty("Position", HSService);

        std::variant<size_t> HSPositionVariant =
            (position < maxPosition) ? (position + 1) : 0;

        auto method =
            bus.new_method_call(HSService.c_str(), HS_DBUS_OBJECT_NAME,
                                phosphor::button::propertyIface, "Set");
        method.append(phosphor::button::hostSelectorIface, "Position");
        method.append(HSPositionVariant);
        auto result = bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Error modifying host selector position",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::debugHSReleased(sdbusplus::message::message& msg)
{
    try
    {
        increaseHSPosition();
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed power process debug host selector button press",
                        entry("ERROR=%s", e.what()));
    }
}

} // namespace button
} // namespace phosphor

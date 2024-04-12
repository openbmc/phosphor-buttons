#include "button_handler.hpp"

#include "config.hpp"
#include "power_button_profile_factory.hpp"

#include <phosphor-logging/lg2.hpp>
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

            // Check for a custom handler
            powerButtonProfile =
                PowerButtonProfileFactory::instance().createProfile(bus);

            if (!powerButtonProfile)
            {
                powerButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                    bus,
                    sdbusRule::type::signal() + sdbusRule::member("Released") +
                        sdbusRule::path(POWER_DBUS_OBJECT_NAME) +
                        sdbusRule::interface(powerButtonIface),
                    std::bind(std::mem_fn(&Handler::powerReleased), this,
                              std::placeholders::_1));
            }
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        // The button wasn't implemented
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
    // return true in case host selector object is available
    return (!getService(HS_DBUS_OBJECT_NAME, hostSelectorIface).empty());
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
    auto hostObjectName = HOST_STATE_OBJECT_NAME + std::to_string(hostNumber);
    auto service = getService(hostObjectName.c_str(), hostIface);
    auto method = bus.new_method_call(service.c_str(), hostObjectName.c_str(),
                                      propertyIface, "Get");
    method.append(hostIface, "CurrentHostState");
    auto result = bus.call(method);

    std::variant<std::string> state;
    result.read(state);

    return Host::HostState::Off !=
           Host::convertHostStateFromString(std::get<std::string>(state));
}

void Handler::handlePowerEvent(PowerEvent powerEventType,
                               std::chrono::microseconds duration)
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
        lg2::info("Multi-host system detected : {POSITION}", "POSITION",
                  hostNumber);
    }

    std::string hostNumStr = std::to_string(hostNumber);

    // ignore  power and reset button events if BMC is selected.
    if (isMultiHostSystem && (hostNumber == BMC_POSITION) &&
        (powerEventType != PowerEvent::powerReleased) &&
        (duration <= LONG_PRESS_TIME_MS))
    {
        lg2::info(
            "handlePowerEvent : BMC selected on multi-host system. ignoring power and reset button events...");
        return;
    }

    switch (powerEventType)
    {
        case PowerEvent::powerReleased:
        {
            if (duration <= LONG_PRESS_TIME_MS)
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

                break;
            }
            else
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
                    lg2::info(
                        "Power is off so ignoring long power button press");
                    return;
                }
                lg2::info("handlePowerEvent : handle long power button press");
                break;
            }
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
#ifdef ENABLE_RESET_BUTTON_DO_WARM_REBOOT
            transition = Host::Transition::ForceWarmReboot;
#else
            transition = Host::Transition::Reboot;
#endif
            break;
        }
        default:
        {
            lg2::error("{EVENT} is invalid power event. skipping...", "EVENT",
                       powerEventType);

            return;
        }
    }
    auto service = getService(objPathName.c_str(), dbusIfaceName);
    auto method = bus.new_method_call(service.c_str(), objPathName.c_str(),
                                      propertyIface, "Set");
    method.append(dbusIfaceName, transitionName, transition);
    bus.call(method);
}
void Handler::powerReleased(sdbusplus::message_t& msg)
{
    try
    {
        uint64_t time;
        msg.read(time);

        handlePowerEvent(PowerEvent::powerReleased,
                         std::chrono::microseconds(time));
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed power state change on a power button press: {ERROR}",
                   "ERROR", e);
    }
}

void Handler::resetReleased(sdbusplus::message_t& /* msg */)
{
    try
    {
        // No need to calculate duration, set to 0.
        handlePowerEvent(PowerEvent::resetReleased,
                         std::chrono::microseconds(0));
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

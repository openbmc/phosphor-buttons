#include "button_handler.hpp"

#include "settings.hpp"

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
    "xyz.openbmc_project.Chassis.Buttons.DebugHostSelector";

constexpr auto propertyIface = "org.freedesktop.DBus.Properties";
constexpr auto mapperIface = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperObjPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";

constexpr auto BMC_POSITION = "0";
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
                std::bind(std::mem_fn(&Handler::powerPressed), this,
                          std::placeholders::_1));

            powerButtonLongPressReleased =
                std::make_unique<sdbusplus::bus::match_t>(
                    bus,
                    sdbusRule::type::signal() +
                        sdbusRule::member("PressedLong") +
                        sdbusRule::path(POWER_DBUS_OBJECT_NAME) +
                        sdbusRule::interface(powerButtonIface),
                    std::bind(std::mem_fn(&Handler::longPowerPressed), this,
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
                std::bind(std::mem_fn(&Handler::idPressed), this,
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
                std::bind(std::mem_fn(&Handler::resetPressed), this,
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
    // return true in case seletor object is available
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

int Handler::getHostSelectorValue()
{
    auto HSService = getService(HS_DBUS_OBJECT_NAME, hostSelectorIface);

    if (HSService.empty())
    {
        log<level::INFO>("Host Selector service not available");
        return -1;
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
        log<level::ERR>("Error reading Host selector Position",
                        entry("ERROR=%s", e.what()));
    }
}

bool Handler::poweredOn(std::string hostIndex) const
{

    auto chassisObjectName = CHASSIS_STATE_OBJECT_NAME + hostIndex;
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

void Handler::powerPressed(sdbusplus::message::message& msg)
{
    auto transition = Host::Transition::On;
    // single host index is zero
    std::string hostIndex = "0";
    try
    {
        if (isMultiHost())
        {
            int hostSelectorPos = getHostSelectorValue();

            if (!hostSelectorPos)
            {
                log<level::INFO>("host sel position is set to BMC or not "
                                 "valid.ignoring power button press");

                return;
            }

            hostIndex = std::to_string(hostSelectorPos);

            auto infoMsg =
                "multi host system detected.Host selector position is : " +
                hostIndex;
            log<level::INFO>(infoMsg.c_str());
        }

        if (poweredOn(hostIndex))
        {
            transition = Host::Transition::Off;
        }

        log<level::INFO>("Handling power button press");

        std::variant<std::string> state = convertForMessage(transition);

        auto hostStateObjectName = HOST_STATE_OBJECT_NAME + hostIndex;

        auto service = getService(hostStateObjectName.c_str(), hostIface);
        auto method = bus.new_method_call(
            service.c_str(), hostStateObjectName.c_str(), propertyIface, "Set");
        method.append(hostIface, "RequestedHostTransition", state);

        bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed power state change on a power button press",
                        entry("ERROR=%s", e.what()));
    }
}
void Handler::doMultiHostSledCycle()
{
    // chassis system reset or sled cycle
    std::variant<std::string> state =
        convertForMessage(Chassis::Transition::PowerCycle);
    auto objPathStr = CHASSISSYSTEM_STATE_OBJECT_NAME + std::to_string(0);
    auto service = getService(objPathStr.c_str(), chassisIface);
    auto method = bus.new_method_call(service.c_str(), objPathStr.c_str(),
                                      propertyIface, "Set");
    method.append(chassisIface, "RequestedPowerTransition", state);
    bus.call(method);
}
void Handler::longPowerPressed(sdbusplus::message::message& msg)
{
    // single host index is zero
    std::string hostIndex = "0";

    try
    {
        if (isMultiHost())
        {
            int hostSelectorPos = getHostSelectorValue();

            if (!hostSelectorPos)
            {
                log<level::INFO>("host sel position is set to BMC or not "
                                 "valid.ignoring long power button press");

                return;
            }

            hostIndex = std::to_string(hostSelectorPos);

            auto infoMsg =
                "multi host system detected.Host selector position is : " +
                hostIndex;
            log<level::INFO>(infoMsg.c_str());

            if (hostIndex == BMC_POSITION)
            {
#if CHASSIS_SYSTEM_RESET_ENABLED
                doMultiHostSledCycle()
#endif
                    return;
            }
        }

        if (!poweredOn(hostIndex))
        {
            log<level::INFO>(
                "Power is off so ignoring long power button press");
            return;
        }

        log<level::INFO>("Handling long power button press");

        std::variant<std::string> state =
            convertForMessage(Chassis::Transition::Off);
        auto chassisStateObjName = CHASSIS_STATE_OBJECT_NAME + hostIndex;
        auto service = getService(chassisStateObjName.c_str(), chassisIface);
        auto method = bus.new_method_call(
            service.c_str(), chassisStateObjName.c_str(), propertyIface, "Set");
        method.append(chassisIface, "RequestedPowerTransition", state);

        bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed powering off on long power button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::resetPressed(sdbusplus::message::message& msg)
{
    // single host index is zero
    std::string hostIndex = "0";

    try
    {

        if (isMultiHost())
        {
            int hostSelectorPos = getHostSelectorValue();

            if (!hostSelectorPos)
            {
                log<level::INFO>("host sel position is set to BMC or not "
                                 "valid.ignoring reset button press");

                return;
            }

            hostIndex = std::to_string(hostSelectorPos);
            auto infoMsg =
                "multi host system detected.Host selector position is : " +
                hostIndex;
            log<level::INFO>(infoMsg.c_str());
        }

        if (!poweredOn(hostIndex))
        {
            log<level::INFO>("Power is off so ignoring reset button press");
            return;
        }

        log<level::INFO>("Handling reset button press");

        std::variant<std::string> state =
            convertForMessage(Host::Transition::Reboot);
        auto hostStateObjName = HOST_STATE_OBJECT_NAME + hostIndex;
        auto service = getService(hostStateObjName.c_str(), hostIface);
        auto method = bus.new_method_call(
            service.c_str(), hostStateObjName.c_str(), propertyIface, "Set");

        method.append(hostIface, "RequestedHostTransition", state);

        bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Failed power state change on a reset button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::idPressed(sdbusplus::message::message& msg)
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
} // namespace button
} // namespace phosphor

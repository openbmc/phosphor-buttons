#include "button_handler.hpp"

#include "settings.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <string>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

namespace phosphor
{
namespace button
{

namespace sdbusRule = sdbusplus::bus::match::rules;
using namespace sdbusplus::xyz::openbmc_project::State::server;
using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;

constexpr auto propertyIface = "org.freedesktop.DBus.Properties";
constexpr auto chassisIface = "xyz.openbmc_project.State.Chassis";
constexpr auto hostIface = "xyz.openbmc_project.State.Host";
constexpr auto powerButtonIface = "xyz.openbmc_project.Chassis.Buttons.Power";
constexpr auto idButtonIface = "xyz.openbmc_project.Chassis.Buttons.ID";
constexpr auto resetButtonIface = "xyz.openbmc_project.Chassis.Buttons.Reset";
constexpr auto selectorButtonIface =
    "xyz.openbmc_project.Chassis.Buttons.Selector";
constexpr auto mapperIface = "xyz.openbmc_project.ObjectMapper";
constexpr auto ledGroupIface = "xyz.openbmc_project.Led.Group";

constexpr auto mapperObjPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto ledGroupBasePath = "/xyz/openbmc_project/led/groups/";

static constexpr auto systemdBusname = "org.freedesktop.systemd1";
static constexpr auto systemdPath = "/org/freedesktop/systemd1";
static constexpr auto systemdInterface = "org.freedesktop.systemd1.Manager";

nlohmann::json appData __attribute__((init_priority(101)));

uint16_t position;
bool debugCardFlag;

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
    catch (SdBusError& e)
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
    catch (SdBusError& e)
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
    catch (SdBusError& e)
    {
        // The button wasn't implemented
    }

    try
    {
        if (!getService(SELECTOR_DBUS_OBJECT_NAME, selectorButtonIface).empty())
        {
            log<level::INFO>("Registering selector button handler");
            selectorButtonReleased = std::make_unique<sdbusplus::bus::match_t>(
                bus,
                sdbusRule::type::signal() + sdbusRule::member("Released") +
                    sdbusRule::path(SELECTOR_DBUS_OBJECT_NAME) +
                    sdbusRule::interface(selectorButtonIface),
                std::bind(std::mem_fn(&Handler::selectorPressed), this,
                          std::placeholders::_1));
        }
    }
    catch (SdBusError& e)
    {
        // The button wasn't implemented
    }

    static std::unique_ptr<sdbusplus::bus::match::match>
        extSelectorButtonSourceMatch = std::make_unique<
            sdbusplus::bus::match::match>(
            bus,
            "type='signal',interface='org.freedesktop.DBus.Properties',"
            "member='PropertiesChanged',arg0namespace='xyz.openbmc_project."
            "Chassis.Buttons."
            "Selector'",
            [&](sdbusplus::message::message& msg) {
                std::string interfaceName;
                boost::container::flat_map<std::string,
                                           std::variant<uint16_t, std::string>>
                    propertiesChanged;
                uint16_t value = 0;
                try
                {
                    msg.read(interfaceName, propertiesChanged);
                    if (propertiesChanged.begin()->first == "Position")
                    {
                        value = std::get<uint16_t>(
                            propertiesChanged.begin()->second);

                        if (value && (!debugCardFlag))
                        {
                            char locstr[10];

                            position = value;
                            sprintf(locstr, "%u", value);
                            setSwPpos(locstr);
                        }
                    }
                }
                catch (std::exception& e)
                {
                    std::cerr
                        << "Unable to read External selector switch position\n";
                }
            });
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

bool Handler::poweredOn() const
{
    auto objPathStr = CHASSIS_STATE_OBJECT_NAME + std::to_string(0);

    auto service = getService(objPathStr.c_str(), chassisIface);
    auto method = bus.new_method_call(service.c_str(), objPathStr.c_str(),
                                      propertyIface, "Get");
    method.append(chassisIface, "CurrentPowerState");
    auto result = bus.call(method);

    std::variant<std::string> state;
    result.read(state);

    return Chassis::PowerState::On ==
           Chassis::convertPowerStateFromString(std::get<std::string>(state));
}

bool Handler::chassisPoweredOn() const
{

    auto objPathStr = CHASSIS_STATE_OBJECT_NAME + std::to_string(position);

    auto service = getService(objPathStr.c_str(), chassisIface);
    auto method = bus.new_method_call(service.c_str(), objPathStr.c_str(),
                                      propertyIface, "Get");
    method.append(chassisIface, "CurrentPowerState");
    auto result = bus.call(method);

    std::variant<std::string> state;
    result.read(state);

    return Chassis::PowerState::On ==
           Chassis::convertPowerStateFromString(std::get<std::string>(state));
}

void Handler::powerPressed(sdbusplus::message::message& msg)
{
    try
    {
        log<level::INFO>("Handling power button press");

#if MULTI_HOST_ENABLED

        if (position != BMC)
        {
            auto transition = Host::Transition::On;

            if (chassisPoweredOn())
            {
                transition = Host::Transition::Off;
            }

            std::variant<std::string> state = convertForMessage(transition);

            auto objPathStr = HOST_STATE_OBJECT_NAME + std::to_string(position);

            auto service = getService(objPathStr.c_str(), hostIface);
            auto method = bus.new_method_call(
                service.c_str(), objPathStr.c_str(), propertyIface, "Set");

            method.append(hostIface, "RequestedHostTransition", state);

            bus.call(method);
        }
#else

        auto transition = Host::Transition::On;

        if (poweredOn())
        {
            transition = Host::Transition::Off;
        }

        std::variant<std::string> state = convertForMessage(transition);

        auto objPathStr = HOST_STATE_OBJECT_NAME + std::to_string(0);

        auto service = getService(objPathStr.c_str(), hostIface);
        auto method = bus.new_method_call(service.c_str(), objPathStr.c_str(),
                                          propertyIface, "Set");
        method.append(hostIface, "RequestedHostTransition", state);

        bus.call(method);
#endif
    }
    catch (SdBusError& e)
    {
        log<level::ERR>("Failed power state change on a power button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::longPowerPressed(sdbusplus::message::message& msg)
{
    try
    {

        log<level::INFO>("Handling long power button press");

#if MULTI_HOST_ENABLED

        if (position != BMC)
        {

            if (!chassisPoweredOn())
            {
                log<level::INFO>(
                    "Power is off so ignoring long power button press");
                return;
            }

            std::variant<std::string> state =
                convertForMessage(Chassis::Transition::Off);

            auto objPathStr =
                CHASSIS_STATE_OBJECT_NAME + std::to_string(position);

            auto service = getService(objPathStr.c_str(), chassisIface);
            auto method = bus.new_method_call(
                service.c_str(), objPathStr.c_str(), propertyIface, "Set");
            method.append(chassisIface, "RequestedPowerTransition", state);

            bus.call(method);
        }
#else
        if (!poweredOn())
        {
            log<level::INFO>(
                "Power is off so ignoring long power button press");
            return;
        }

        std::variant<std::string> state =
            convertForMessage(Chassis::Transition::On);

        auto objPathStr = CHASSIS_STATE_OBJECT_NAME + std::to_string(0);

        auto service = getService(objPathStr.c_str(), chassisIface);
        auto method = bus.new_method_call(service.c_str(), objPathStr.c_str(),
                                          propertyIface, "Set");
        method.append(chassisIface, "RequestedPowerTransition", state);

        bus.call(method);

#endif

#if CHASSIS_SYSTEM_RESET_ENABLED
        if (position == BMC)
        {
            // chassis system reset or sled cycle

            std::variant<std::string> state =
                convertForMessage(Chassis::Transition::PowerCycle);

            auto objPathStr =
                CHASSISSYSTEM_STATE_OBJECT_NAME + std::to_string(0);

            auto service = getService(objPathStr.c_str(), chassisIface);

            auto method = bus.new_method_call(
                service.c_str(), objPathStr.c_str(), propertyIface, "Set");
            method.append(chassisIface, "RequestedPowerTransition", state);

            bus.call(method);
        }
#endif
    }
    catch (SdBusError& e)
    {
        log<level::ERR>("Failed powering off on long power button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::resetPressed(sdbusplus::message::message& msg)
{
    try
    {

        log<level::INFO>("Handling reset button press");
#if MULTI_HOST_ENABLED
        if (position != BMC)
        {
            if (!chassisPoweredOn())
            {
                log<level::INFO>("Power is off so ignoring reset button press");
                return;
            }

            std::variant<std::string> state =
                "xyz.openbmc_project.State.Host.Transition.ForceWarmReboot";

            auto objPathStr = HOST_STATE_OBJECT_NAME + std::to_string(position);

            auto service = getService(objPathStr.c_str(), hostIface);

            auto method = bus.new_method_call(
                service.c_str(), objPathStr.c_str(), propertyIface, "Set");
            method.append(hostIface, "RequestedHostTransition", state);

            bus.call(method);
        }
#else
        if (!poweredOn())
        {
            log<level::INFO>("Power is off so ignoring reset button press");
            return;
        }

        std::variant<std::string> state =
            convertForMessage(Host::Transition::Reboot);

        auto objPathStr = HOST_STATE_OBJECT_NAME + std::to_string(0);

        auto service = getService(objPathStr.c_str(), hostIface);

        method.append(hostIface, "RequestedHostTransition", state);

        bus.call(method);
#endif
    }
    catch (SdBusError& e)
    {
        log<level::ERR>("Failed power state change on a reset button press",
                        entry("ERROR=%s", e.what()));
    }
}

bool Handler::getSwPpos(char* pos)
{
    /* Get position data stored in json file */
    std::ifstream file(HOST_POS_PATH);
    if (file)
    {
        file >> appData;
        file.close();
    }
    else
    {
        return 1;
    }
    std::string str = appData[KEY].get<std::string>();

    *pos++ = 0; // byte 1: Set selector not supported
    *pos++ = 0; // byte 2: Only ASCII supported

    int len = str.length();
    *pos++ = len;
    memcpy(pos, str.data(), len);

    return 0;
}

bool Handler::setSwPpos(char* pos)
{
    std::stringstream ss;

    ss << (char)pos[0];

    appData[KEY] = ss.str();

    std::ofstream file(HOST_POS_PATH);
    file << appData;
    file.close();

    return 0;
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
    catch (SdBusError& e)
    {
        log<level::ERR>("Error toggling ID LED group on ID button press",
                        entry("ERROR=%s", e.what()));
    }
}

void Handler::startExtPlatService(void)
{
    static boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);

    auto method = conn->new_method_call(systemdBusname, systemdPath,
                                        systemdInterface, "StartUnit");
    auto ServiceFile = "hand_switch_position_sol.service";
    method.append(ServiceFile, "replace");
    try
    {
        conn->call_noreply(method);
        log<level::INFO>("started platform specific service");
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("failed to start platform specific service.",
                        entry("ERR=%s", e.what()));
    }
}

void Handler::selectorPressed(sdbusplus::message::message& msg)
{
    static boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    char locstr[10];

    if (!getSwPpos(&locstr[0]))
    {
        position = atoi(&locstr[3]);
    }
    else
    {
        position = SERVER1;
    }

    position = position >= BMC ? SERVER1 : (position + 1);

    debugCardFlag = 1;

    try
    {
        auto method = conn->new_method_call(
            "xyz.openbmc_project.Chassis.Buttons",
            "/xyz/openbmc_project/Chassis/Buttons/Selector0",
            "org.freedesktop.DBus.Properties", "Set");

        method.append("xyz.openbmc_project.Chassis.Buttons.Selector",
                      "Position", std::variant<uint16_t>(position));

        // Invoke method call function
        auto reply = conn->call(method);
        if (reply.is_method_error())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error setting selectorswitch position");
        }
    }
    catch (SdBusError& e)
    {
        log<level::ERR>("Error setting selector switch position",
                        entry("ERROR=%s", e.what()));
    }

    startExtPlatService();

    sprintf(locstr, "%u", position);
    setSwPpos(locstr);

    debugCardFlag = 0;
}

} // namespace button
} // namespace phosphor

#pragma once
#include "config.h"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>

#include <sstream>
#include <string>

namespace phosphor
{
namespace button
{
enum class PowerEvent
{
    powerPressed,
    longPowerPressed,
    longerPowerPressed,
    resetPressed,
    powerReleased,
    longPowerReleased,
    resetReleased
};

inline static int numberOfChassis()
{
    int chassisNumFind;
    size_t found = 0;

    for (int i = 0; found != std::string::npos; i++)
    {
        found = static_cast<std::string>(INSTANCES).find(std::to_string(i));
        chassisNumFind = i - 1;
    }

    return chassisNumFind;
}

/**
 * @class Handler
 *
 * This class acts on the signals generated by the
 * xyz.openbmc_project.Chassis.Buttons code when
 * it detects button presses.
 *
 * There are 3 buttons supported - Power, ID, and Reset.
 * As not all systems may implement each button, this class will
 * check for that button on D-Bus before listening for its signals.
 */
class Handler
{
  public:
    Handler() = delete;
    ~Handler() = default;
    Handler(const Handler&) = delete;
    Handler& operator=(const Handler&) = delete;
    Handler(Handler&&) = delete;
    Handler& operator=(Handler&&) = delete;

    /**
     * @brief Constructor
     *
     * @param[in] bus - sdbusplus connection object
     */
    explicit Handler(sdbusplus::bus_t& bus);

  private:
    /**
     * @brief The handler for a power button press
     *
     * It will power on the system if it's currently off,
     * else it will soft power it off.
     *
     * @param[in] msg - sdbusplus message from signal
     */
    void powerReleased(sdbusplus::message_t& msg);

    /**
     * @brief The handler for an ID button press
     *
     * Toggles the ID LED group
     *
     * @param[in] msg - sdbusplus message from signal
     */
    void idReleased(sdbusplus::message_t& msg);

    /**
     * @brief The handler for a reset button press
     *
     * Reboots the host if it is powered on.
     *
     * @param[in] msg - sdbusplus message from signal
     */
    void resetReleased(sdbusplus::message_t& msg);

    /**
     * @brief The handler for a OCP debug card host selector button press
     *
     * In multi host system increases host position by 1 up to max host
     * position.
     *
     * @param[in] msg - sdbusplus message from signal
     */

    void debugHostSelectorReleased(sdbusplus::message_t& msg);

    /**
     * @brief Checks if system is powered on
     *
     * @return true if powered on, false else
     */
    bool poweredOn(size_t hostNumber) const;

    /*
     * @return std::string - the D-Bus service name if found, else
     *                       an empty string
     */
    std::string getService(const std::string& path,
                           const std::string& interface) const;

    /**
     * @brief gets the valid host selector value in multi host
     * system
     *
     * @return size_t throws exception if host selector position is
     * invalid or not available.
     */

    size_t getHostSelectorValue();
    /**
     * @brief increases the host selector position property
     * by 1 upto max host selector position
     *
     * @return void
     */

    void increaseHostSelectorPosition();
    /**
     * @brief checks if the system has multi host
     * based on the host selector property availability
     *
     * @return bool returns true if multi host system
     * else returns false.
     */
    bool isMultiHost();
    /**
     * @brief trigger the power ctrl event based on the
     *  button press event type.
     *
     * @return void
     */
    void handlePowerEvent(PowerEvent powerEventType, std::string chassisNum);

    /**
     * @brief sdbusplus connection object
     */
    sdbusplus::bus_t& bus;

    /**
     * @brief Matches on the power button released signal
     */
    std::unique_ptr<sdbusplus::bus::match_t> powerButtonReleased;

    /**
     * @brief Matches on the power button long press released signal
     */
    std::unique_ptr<sdbusplus::bus::match_t> powerButtonLongPressed;

    /**
     * @brief Matches on the multi power button released signal
     */
    std::vector<std::shared_ptr<sdbusplus::bus::match_t>>
        multiPowerButtonReleased;

    /**
     * @brief Matches on the ID button released signal
     */
    std::unique_ptr<sdbusplus::bus::match_t> idButtonReleased;

    /**
     * @brief Matches on the reset button released signal
     */
    std::unique_ptr<sdbusplus::bus::match_t> resetButtonReleased;

    /**
     * @brief Matches on the ocp debug host selector  button released signal
     */
    std::unique_ptr<sdbusplus::bus::match_t> debugHSButtonReleased;
};

} // namespace button
} // namespace phosphor

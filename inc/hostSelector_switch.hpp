#pragma once
#include "config.h"

#include "button_factory.hpp"
#include "button_interface.hpp"
#include "common.hpp"
#include "gpio.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/HostSelector/server.hpp"
#include "xyz/openbmc_project/Chassis/Common/error.hpp"

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iostream>
extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

static constexpr std::string_view HOST_SELECTOR = "HOST_SELECTOR";
using HostSelectorIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::HostSelector>;
static constexpr auto INVALID_INDEX = std::numeric_limits<size_t>::max();

class HostSelector final : public HostSelectorIntf, public ButtonIface
{
  public:
    HostSelector(sdbusplus::bus_t& bus, const char* path, EventPtr& event,
                 buttonConfig& buttonCfg) :
        HostSelectorIntf(bus, path, action::defer_emit),
        ButtonIface(bus, event, buttonCfg)
    {
        init();
        // read and store the host selector position Map
        hsPosMap = buttonCfg.extraJsonInfo.at("host_selector_map")
                       .get<std::map<std::string, int>>();
        maxPosition(buttonCfg.extraJsonInfo["max_position"], true);
        type = buttonCfg.extraJsonInfo.at("type");

        if (type == "i2c")
        {
            size_t busId = buttonCfg.extraJsonInfo.at("busId");
            offset = buttonCfg.extraJsonInfo.at("offset");
            deviceAddress = buttonCfg.extraJsonInfo.at("address");

            i2cBus = "/dev/i2c-" + std::to_string(busId);
        }
        else
        {
            gpioLineCount = buttonCfg.gpios.size();
            setInitialHostSelectorValue();
        }
        emit_object_added();
    }

    ~HostSelector()
    {
        deInit();
    }

    static constexpr std::string_view getFormFactorName()
    {
        return HOST_SELECTOR;
    }

    static const char* getDbusObjectPath()
    {
        return HS_DBUS_OBJECT_NAME;
    }
    void handleEvent(sd_event_source* es, int fd, uint32_t revents) override;
    size_t getMappedHSConfig(size_t hsPosition);
    size_t getGpioIndex(int fd);
    void setInitialHostSelectorValue(void);
    void setHostSelectorValue(int fd, GpioState state);
    using HostSelectorIntf::position;
    size_t position() const override
    {
        if (type == "i2c")
        {
            int i2cFd = open(i2cBus.c_str(), O_RDWR);
            int error = 0;
            int value = 0;
            if (i2cFd < 0)
            {
                lg2::error("{TYPE}: i2c fd open error: {ERROR}", "TYPE",
                           getFormFactorType(), "ERROR", i2cFd);
                error++;
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            if (ioctl(i2cFd, I2C_SLAVE_FORCE, deviceAddress) < 0)
            {
                lg2::error("{TYPE}:cannot set i2c address: {ERROR}", "TYPE",
                           getFormFactorType(), "ERROR", i2cFd);
                close(i2cFd);
                error++;
            }
            value = i2c_smbus_read_byte_data(i2cFd, offset);
            if (value < 0)
            {
                lg2::error("{TYPE}: i2c read failed: {ERROR}", "TYPE",
                           getFormFactorType(), "ERROR", value);
                close(i2cFd);
                error++;
            }
            close(i2cFd);
            if (error)
            {
                lg2::error("{TYPE}:i2c read rror: {ERROR}", "TYPE",
                           getFormFactorType(), "ERROR", errno);
                value = 0;
            }
            return value;
        }
        return HostSelectorIntf::position();
    }

  protected:
    size_t hostSelectorPosition = 0;
    size_t gpioLineCount;
    uint8_t deviceAddress;
    uint8_t offset;
    std::string i2cBus;
    std::string type;
    // map of read Host selector switch value and corresponding host number
    // value.
    std::map<std::string, int> hsPosMap;
};

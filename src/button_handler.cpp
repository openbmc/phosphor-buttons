#include "button_handler.hpp"

namespace phosphor
{
namespace button
{
Handler::Handler(sdbusplus::bus::bus& bus) : bus(bus)
{
}
}
}

#include "button_handler.hpp"
#include <sdeventplus/event.hpp>

int main(void)
{
    auto bus = sdbusplus::bus::new_default();
    auto event = sdeventplus::Event::get_default();

    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    phosphor::button::Handler handler{bus};

    return event.loop();
}

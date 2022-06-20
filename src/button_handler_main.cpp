#include "button_handler.hpp"

int main(void)
{
    auto bus = sdbusplus::bus::new_default();

    phosphor::button::Handler handler{bus};

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}

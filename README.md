# phosphor-buttons

Phosphor-buttons has a collection of IO event handler interfaces
for physical inputs which are part of OCP front panel.

It defines an individual dbus interface object for each physical
button/switch inputs such as power button, reset button etc.
Each of this button interfaces monitors it's associated io for event changes and calls
the respective event handlers.

## Gpio defs config
    In order to monitor a button/input interface the
respective gpio config details should be mentioned in the
gpio defs json file - "/etc/default/obmc/gpio/gpio_defs.json"

 1. The button interface type name.
 2. An array consists of single or multiple
    gpio configs associated with the specific button interface.

## example gpio def Json config

{
    "gpio_definitions": [
        {
            "name": "POWER_BUTTON",
            "gpio_config" :[
               {
                "pin": "D0",
                "direction": "both"
               }
            ]
        },
        {
            "name": "RESET_BUTTON",
            "gpio_config" :[
                {
                "pin": "AB0",
                "direction": "both"
                 }
            ]
        },
        {
            "name" : "HOST_SELECTOR",

            "gpio_config" : [
            {
                "pin": "AA4",
                "direction": "both"
            },
            {
                "pin": "AA5",
                "direction": "both"
            },
            {
                "pin": "AA6",
                "direction": "both"
            },
            {
                "pin": "AA7",
                "direction": "both"
            }
            ]
        },

}

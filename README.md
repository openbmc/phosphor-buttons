# phosphor-buttons

Phosphor-buttons has a collection of IO event handler interfaces for physical
inputs which are part of OCP front panel.

It defines an individual dbus interface object for each physical button/switch
inputs such as power button, reset button etc. Each of this button interfaces
monitors it's associated io for event changes and calls the respective event
handlers.

## Gpio defs config

In order to monitor a button/input interface the respective gpio config details
should be mentioned in the gpio defs json file -
`/etc/default/obmc/gpio/gpio_defs.json`

1. The button interface type name.
2. An array consists of single or multiple gpio configs associated with the
   specific button interface.

## example gpio def Json config

```json
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

            "group_gpio_config" : [
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
```

## Single gpio config

This config is original config which can be used for configs with only single
gpio such as power button,reset button, OCP debug card host select button.

```json
{
    "name": "POWER_BUTTON",
    "pin": "D0",
    "direction": "both"
},
```

**Note:** this config is used by most of the other platforms so this format is
kept as it is so that existing gpio configs do not get affected.

## Group gpio config

The following configs are related to multi-host bmc systems more info explained
in the design:
<https://github.com/openbmc/docs/blob/master/designs/multihost-phosphor-buttons.md>

### Host selector gpio config example

The host selector has four gpios associated. So the related gpios are mentioned
in a json array named group_gpio_config.

- name - This is name of the specific gpio line
- pin - This represents the pin number from linux dts file.
- polarity - polarity type of the gpio
- max_position - This represents the max number of hosts in the multi-host bmc
  system.
- host_selector_map - This map is oem specific host position map which has how
  the value read from the host selector gpios is mapped to the respective host
  number.

Example : The value of "7" derived from the 4 host select gpio lines are mapped
to host position 1.

```json
{
  "name": "HOST_SELECTOR",

  "group_gpio_config": [
    {
      "name": "host_select_0",
      "pin": "AA4",
      "direction": "both",
      "polarity": "active_high"
    },
    {
      "name": "host_select_1",
      "pin": "AA5",
      "direction": "both",
      "polarity": "active_high"
    },
    {
      "name": "host_select_2",
      "pin": "AA6",
      "direction": "both",
      "polarity": "active_high"
    },
    {
      "name": "host_select_3",
      "pin": "AA7",
      "direction": "both",
      "polarity": "active_high"
    }
  ],
  "max_position": 4,
  "host_selector_map": {
    "6": 0,
    "7": 1,
    "8": 2,
    "9": 3,
    "10": 4,
    "11": 0,
    "12": 1,
    "13": 2,
    "14": 3,
    "15": 4
  }
}
```

### Serial uart mux config

Similar to host selector there are multiple gpios associated with the serial
uart mux. These gpio configs are specificed as part of json array
"group_gpio_config".

Here the serial uart mux output is accessed via OCP debug card. SO the OCP debug
card present gpio is mentioned part of the group_gpio_config. The debug card
present gpio is identified by its name "debug_card_present".

The other gpios part of the group gpio config is serial uart MUX gpio select
lines and serial_uart_rx line.

- name - this is name of the specific gpio line
- pin - this represents the pin number from linux dts file.
- polarity - polarity type of the gpio
- serial_uart_mux_map - This is the map for selected host position to the serial
  uart mux select output value

```json
{
  "name": "SERIAL_UART_MUX",

  "group_gpio_config": [
    {
      "name": "serial_uart_sel_0",
      "pin": "E0",
      "direction": "out",
      "polarity": "active_high"
    },
    {
      "name": "serial_uart_sel_1",
      "pin": "E1",
      "direction": "out",
      "polarity": "active_high"
    },
    {
      "name": "serial_uart_sel_2",
      "pin": "E2",
      "direction": "out",
      "polarity": "active_high"
    },
    {
      "name": "serial_uart_sel_3",
      "pin": "E3",
      "direction": "out",
      "polarity": "active_high"
    },
    {
      "name": "serial_uart_rx",
      "pin": "E4",
      "direction": "out",
      "polarity": "active_high"
    },
    {
      "name": "debug_card_present",
      "pin": "R3",
      "direction": "both",
      "polarity": "active_high"
    }
  ],
  "serial_uart_mux_map": {
    "0": 4,
    "1": 0,
    "2": 1,
    "3": 2,
    "4": 3
  }
}
```
## Multiple power gpio config example

This config is used for configs with multiple power buttons on each slots and whole sled,
or for which needs multi-level chassis power control behavior.

- name - this is name of the specific gpio line, "POWER_BUTTON" + the index of chassis instance
- pin - this represents the pin number from linux dts file.
- multi-action - default to no action, set the corresponding behavior with following format:
 - duration - activate when pulling gpio over this value (unit: millisecond) 
 - action - chassis power control behavior

```json
{
    "gpio_definitions": [
        {
            "name": "POWER_BUTTON0",
            "pin": "I6",
            "direction": "both",
            "multi-action": [
                {
                    "duration": 4000,
                    "action": "chassis cycle"
                }
            ]
        },
        {
            "name": "POWER_BUTTON1",
            "pin": "V0",
            "direction": "both",
            "multi-action": [
                {
                    "duration": 0,
                    "action": "chassis on"
                },
                {
                    "duration": 4000,
                    "action": "chassis cycle"
                },
                {
                    "duration": 8000,
                    "action": "chassis off"
                }
            ]
        }
    ]
}
```

# PmgBatteryCharger Middleware Library 1.0

## Overview

The PMG Battery Charger is ModusToolbox middleware for a USB-C battery charger
using a buck-boost power stage. It provides charger policy, battery monitoring,
power-stage control, and hardware protection support.


## Features

* Sink-mode charging with pre-charge, normal-charge, full-charge, and optional
  top-off operation
* Source-mode operation with battery discharge monitoring
* Charger state machine for initialization, soft-start, charging, shutdown,
  cooldown, and source operation
* ADC measurement of battery voltage, input/output current, voltage rails, and
  temperature sensors
* PWM control of the buck-boost power stage
* Battery OVP, UVP, OCP, OTP, timeout, thermal, and buck-boost error handling
* Input/output FET, buck-boost enable/reset, rectifier, and discharge control
* Optional UART debug output

## Quick Start

1. Add the PMG Battery Charger source and header files to a ModusToolbox
   application with the required PDStack, PDUtils, USBPD, and device PDL
   dependencies.
2. Populate a `cy_stc_battery_charger_cfg_t` structure with the battery cell
   count, voltage and current limits, timeout settings, thermal thresholds, and
   post-charge action.
3. Initialize the middleware with `Cy_App_Pmg_Init()` and initialize the board
   hardware with `Cy_PmgChg_HW_Init()`.
4. Forward PD notifications to `Cy_App_Pmg_PdEventHandler()` and call
   `Cy_App_Pmg_Task()` from the application processing loop.
5. Use `Cy_App_Pmg_Charging_Disable()` to stop charging when required. The
   current status and measurements are available through
   `get_battery_charging_context()`.

See the [API Reference Guide](https://infineon.github.io/pmg-battery-charger/html/index.html)
for the generated API documentation.


## Related resources

Resources    | Links
-------------|------------------------------------------------------------------
Libraries on GitHub | [mtb-pdl-cat2](https://github.com/Infineon/mtb-pdl-cat2) – Peripheral Driver Library (PDL) and docs
Middleware on GitHub | [pdstack](https://github.com/Infineon/pdstack) – PDStack middleware library and docs 
Tools | [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) – ModusToolbox&trade; software is a collection of easy-to-use libraries and tools enabling rapid development with Infineon MCUs for applications ranging from wireless and cloud-connected systems, edge AI/ML, embedded sense and control, to wired USB connectivity using PSOC&trade; Industrial/IoT MCUs, AIROC&trade; Wi-Fi and Bluetooth&reg; connectivity devices, XMC&trade; Industrial MCUs, and EZ-USB&trade;/EZ-PD&trade; wired connectivity controllers. ModusToolbox&trade; incorporates a comprehensive set of BSPs, HAL, libraries, configuration tools, and provides support for industry-standard IDEs to fast-track your embedded application development.

## More information

For more information, refer to the following documents:

* [PMGBatteryCharger API reference guide](https://infineon.github.io/pmg-battery-charger/html/index.html)
* [ModusToolbox&trade;, quick start guide, documentation, and videos](https://www.infineon.com/modustoolbox)
* [Infineon Technologies AG](https://www.infineon.com)

---
© 2026, Infineon Technologies AG or an affiliate of Infineon Technologies AG.

# Irrigation Management

This is an irrigation management system based on the ESP8266 platform.

## Hardware

An MCP23017 (I2C) is used control LEDs and relais, because the system is able to control
two pumps, six valves and has three LEDs which is in total 16 GPIOs which is more than
the board provides.

## Build

A file `settings.h` contains all `#define` commands for base properties (e.g. WIFI, etc.).
Add this file by copying this template and adapt the values according to your environment:

```c
#define WIFI_SSID "XXXXXXXXX"
#define WIFI_PASSWORD "YYYYYYYYY"
```

If your using a mesh wifi you have to define the port and the MAC address of the access
point you want to connect to. Both can be determined by using a "Wifi Monitor" app.

```c
#define WIFI_PORT 11
#define WIFI_MAC { 0x1C, 0x7F, 0x2C, 0x63, 0xA3, 0x58 }
```

Hint: There are connectivity issues for port greater than 11.

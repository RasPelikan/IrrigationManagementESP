# Irrigation Management

This is an irrigation management system based on the ESP8266 platform.

It controls a low rate well pump which pumps water into containers during the day.
Additionally, it controls a irrigation pump (placed in the container) based on
the pipes current water pressure and the container's current level of water.

This setup is needed if your well feels not well any more ;-) - means there is
water but not enough to place the high rated irrigation pump directly into the
well.

## Hardware

An MCP23017 (I2C) is used control LEDs and relais, because the system is able to control
two pumps, six valves and has three LEDs which is in total 16 GPIOs which is more than
the board provides.

## Build

A file `settings.h` contains all `#define` commands for base properties (e.g. WIFI, etc.).
Add this file by copying this template and adapt the values according to your environment.

## settings.h

### Wifi

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

Hint: There are [connectivity issues](https://olimex.wordpress.com/2021/12/10/avoid-wifi-channel-12-13-14-when-working-with-esp-devices/) for port greater than 11.

### Well pump interval

Most low rate well pumps are not supposed to run 24h without any break.
Typically, an cycle is given which needs to be configured like this:

```c
#define PUMP_WELL_SLEEP 15  // 15/45 minute cycle
#define PUMP_WELL_RUN 45    // 45/15 minute cycle
```
### Water sensor hysteresis

Once a changed water level is detected it is necessary to pause sensing for update
because waves in the container might cause fluctuation messurements. In electronic
terms this is called hysteresis. Set a proper value according to the size of your
container: For bigger containers it takes more time to pump enough water so waves
cause this issue.

```c
#define WATERLEVEL_HYSTERESIS 120  // 120 seconds = 2 minutes
````

### Water pressure sensor hysteresis

The water pressure is measured using ADC input (values from 0 to 1023). One can define
the lower and upper boundary and the hysteresis: how long the irrigation pump will
stay switched off once is has been switch off.

```c
#define WATERPRESSURE_LOW_END 200
#define WATERPRESSURE_LOW_HYSTERESIS 5 // disable switching off for 5 seconds
#define WATERPRESSURE_HIGH_END 600
#define WATERPRESSURE_HIGH_HYSTERESIS 20 // disable switching on for 2 minutes
````

Hint: There is [data available](./readme/sensor-values.xlsx) for this sensor:
[https://www.amazon.de/dp/B07SYLH59Q](https://www.amazon.de/dp/B07SYLH59Q).

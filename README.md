# Irrigation Management

This is an irrigation management system based on the ESP8266 platform.

Features:
1. It controls a low rate well pump which pumps water into containers during the day.
1. It controls an irrigation pump (placed in the container) based on the pipes current water pressure and the container's current level of water.
1. It irrigates by switching valves according to configured irrigation cycles.
1. Irrigation cycles may be fixed (irrigate the same in every cycle) or rolling (irrigate configured areas beginning were stopped last time).
1. Valves may also be switch remote by calling a defined URL. So the main devices does not necessarily be connected to all valves.
1. Provide a webapp to control the device.

This two pumps setup is needed if your well feels not well any more ;-) - means there is
water but not enough to place the high rated irrigation pump directly into the
well.

## Hardware

The firmware is compatible to any [NodeMCU modul](https://www.amazon.de/dp/B06Y1ZPNMS) or
[Mini module with external antennas](https://www.amazon.de/dp/B0CT9K2XHK).

An MCP23017 (I2C) is used control LEDs and relais, because the system is able to control
two pumps, six valves and has three LEDs which is in total 16 GPIOs which is more than
the board provides.

For measurment of pressure a [sensor](https://www.amazon.de/dp/B07SYLH59Q) ([sensor values](./readme/sensor-values.xlsx))
is used connected to the ADC pin.

## Build

This project is meant to be opened in ArduinoIDE. One needs to add libraries:

1. `ESP Async WebServer` (Version 3.7.9)
1. `ESP Async TCP` (Version 2.0.0)
1. `ElegantOTA` (Version 3.1.7, turned to [async mode](https://docs.elegantota.pro/getting-started/async-mode))

The webapp included has to be built like this manually:

```shell
cd webapp
npm install
npm run build
cd ..
```

## Configuration file

There is a template for the configuration file: `config-template.json`. To upload your
individual configuration file, make a new directory `data` in this project (it's already
excluded but `.gitignore`) and place a copy of the template named als `config.json` into
the new directory. Also the webapp (needs to be built before) is placed there for transfering to the device.

Now you can use
[Arduino IDE ESP8266 LittleFS Filessystem Uploader Plugin](https://randomnerdtutorials.com/arduino-ide-2-install-esp8266-littlefs/)
to send the file to your board.

Hint: There are [connectivity issues](https://olimex.wordpress.com/2021/12/10/avoid-wifi-channel-12-13-14-when-working-with-esp-devices/) for port greater than 11.

## config-template.json

### Wifi

```json
{
  "wifi": {
    "ssid": "YourWifiSSID",
    "password": "YourWifiPassword",
    "port": 11
  },
  "http": {
    "username": "im",
    "password": "Secure_123"
  },
  ...
}```

`wifi.ssid` and `wifi.password` is mandatory. `wifi.port` is optional and can be used to choose
a specific access point by it's port.

If `http` section is given, then the web application will be protected using the credentials given
in `http.username` and `http.password`.

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

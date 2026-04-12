# Irrigation Management

This is an irrigation management system based on the ESP32 platform.

Features:
1. It controls a low rate well pump which pumps water into containers during the day.
1. It controls an irrigation pump (placed in the container) based on the pipes current water pressure and the container's current level of water.
1. It irrigates by switching valves according to configured irrigation cycles.
1. Irrigation cycles may be fixed (irrigate the same in every cycle) or rolling (irrigate configured areas beginning were stopped last time).
1. Valves may also be switch remote by calling a defined URL. So the main devices does not necessarily be connected to all valves (see [client](https://github.com/RasPelikan/IrrigationClientESP)).
1. Provide a webapp to control the device.

This two pumps setup is needed if your well feels not well any more ;-) - means there is
water but not enough to place the high rated irrigation pump directly into the
well.

## Hardware

The firmware is compatible to any ESP32 Dev Module, e.g. an
[ESP32-DevKitC](https://www.amazon.de/dp/B071P98VTG) or an ESP32U variant with external antenna.

The ESP32 has enough GPIOs to directly control LEDs, relays and sensors without a port expander:

| GPIO | Function | Direction |
|------|----------|-----------|
| 34 | Water pressure sensor (ADC) | Input |
| 33 | Irrigation pump relay | Output |
| 32 | Well pump relay | Output |
| 25 | Valve 1 relay | Output |
| 26 | Valve 2 relay | Output |
| 27 | Valve 3 relay | Output |
| 14 | Valve 4 relay | Output |
| 13 | Valve 5 relay | Output |
| 19 | Water level: empty | Input (pullup) |
| 18 | Water level: 1 | Input (pullup) |
| 5 | Water level: 2 | Input (pullup) |
| 17 | Water level: 3 | Input (pullup) |
| 16 | Water level: full | Input (pullup) |
| 21 | LED: WiFi | Output |
| 23 | LED: Well pump | Output |
| 22 | LED: Irrigation pump | Output |

For measurement of pressure a [sensor](https://www.amazon.de/dp/B07SYLH59Q) ([sensor values](./readme/sensor-values.xlsx))
is used connected to the ADC pin (GPIO 34). The ESP32 ADC is 12-bit but is set to 10-bit (values 0–1023).

## Build

This project is meant to be opened in ArduinoIDE.

### Board settings

- **Board:** `ESP32 Dev Module`
- **Flash Size:** 4MB
- **Partition Scheme:** Default 4MB with spiffs
- **Upload Speed:** 921600

If the ESP32 board package is not yet installed:
1. Arduino IDE → Preferences → Additional Board Manager URLs:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Tools → Board Manager → search "esp32" → install **esp32 by Espressif Systems**

### Required libraries

1. `AsyncTCP`
1. `ESPAsyncWebServer` (ESP32 version)
1. `ElegantOTA` (Version 3.1.7, turned to [async mode](https://docs.elegantota.pro/getting-started/async-mode))
1. `ArduinoJson` (Version 7.4.2)

### Webapp

The webapp included has to be built like this manually:

```shell
cd webapp
npm install
npm run build
cd ..
```

### Initial upload

Initially, the firmware and the files (webapp and config files) have to be uploaded via USB.
Once the device is available via HTTP over the air (OTA) updates of the firmware can be done via
URL `/update`. The configuration can be modified and also the webapp itself can be updated
via webapp.

Once created initial config files (see section [Configuration files](#configuration-files))
and the webapp, one can use the
[Arduino IDE ESP32 LittleFS Filesystem Uploader Plugin](https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/)
to send all files to your board (Serial console has to be closed during upload!).

## Configuration files

There are two configuration files: `credentials.json` and `config.json`. The `config.json` can be modified
via webapp. The `credentials.json` has to be uploaded via USB.

### Credentials

Create a file `/data/credentials.json` and use this as a template:

```json
{
  "wifi": {
    "ssid": "Your-Wifi-SSID",
    "password": "Your-Wifi-Password"
  },
  "http": {
    "username": "Your-Webapp-Username",
    "password": "Your-Webapp-Password"
  }
}
```

### Configuration

Create a file `/data/config.json` and use this as a template:

```json
{
  "wifi": {                 // optional
    "channel": 9            // the wifi channel to connect to (may be useful in case of several access points)
  },
  "pumps": {
    "well": {               // well-pump specific config
      "cycle": {            // the cycle in to pump water
                            // typically pumps are not meant to run without break
        "on": 45,           // minutes how long to pump
        "off": 15           // minutes of the break
      }
    },
    "irrigation": {         // irrigation specific config
      "hysteresis": 1200    // seconds how long to pause once the pump was switch off
                            // typically pumps are only allowed to start x times per hour
    }
  },
  "water": {
    "level": {              // water-level specific config
      "hysteresis": 120     // once a changed water level is detected it is necessary to pause sensing for update
                            // because waves in the container might cause fluctuation messurements. In electronic
                            // terms this is called hysteresis. Set a proper value according to the size of your
                            // container: For bigger containers it takes more time to pump enough water so waves
                            // cause this issue. The unit is seconds.
    },
    "pressure": {           // water-pressure specific config
      "low": 3.0,           // the low end in bar to start the irrigation pump
      "high": 5.5,          // the high end in bar to stop the irrigation pump
      "reference": {        // to setup ADC for pressure sensor
        "low": {            // pump water at pressure of e.g. 1 bar into the system (don't use 0 bar for 'low')
          "bar": 1.0,       // the exact pressure one can read from analog sensor
          "value": 1000     // the ADC value shown in the webapp (values from 0 to 4095 on ESP32)
        },
        "high": {           // pump water at pressure of e.g. 5 bar into the system
          "bar": 5.0,       // the exact pressure one can read from analog sensor
          "value": 3200     // the ADC value shown in the webapp (values from 0 to 4095 on ESP32)
        }
      }
    }
  },
  "valves": [               // valves available in the entire system
    {
     "id": "local1",        // a valves ID
     "gpio": 1              // the GPIO number in case of controlling via GPIO
    },
    {
     "id": "remote1",       // also remote valves are supported:
     "remote": "http://10.0.0.47/valve/1"
                            // a POST request is sent every minute to activate the valve
                            // having body "active=true" or "active=false". The remote pump
                            // should switch off once there is no request for 90 seconds.
    }
  ],
  "areas": {                  // areas in the garden for irrigation
    "road": {                 // name of the area
     "reset": true,           // wether sequences should start at the beginning in each cycle
     "sequence": [            // sequences of irrigation this area
      {
       "duration": 2,         // duration in minutes
       "valves": ["local1"]   // valves to be activated
      },
      {
       "duration": 2,
       "valves": ["remote1"]
      }
     ]
    },
    "lawn": {
     "reset": false,
     "sequence": [
       {
         "duration": 6,
         "valves": ["local1"]
       },
       {
         "duration": 6,
         "valves": ["local1", "remote1"]
       },
       {
         "duration": 6,
         "valves": ["remote1"]
       }
     ]
    }
  },
  "cycles": [               // cycles to irrigate areas
    {
     "start": "0530",       // time of start
     "end": "0540",         // time of end
     "area": "road"         // area to irrigate
    },
    {
     "start": "2200",
     "end": "2210",
     "area": "road"
    },
    {
     "start": "2210",
     "end": "2300",
     "area": "lawn"
    },
    {
     "start": "0500",
     "end": "0530",
     "area": "lawn"
    }
  ]
}
```

Hints:

1. Strip comments from JSON, otherwise parser will fail.
1. There are WIFI [connectivity issues](https://olimex.wordpress.com/2021/12/10/avoid-wifi-channel-12-13-14-when-working-with-esp-devices/) for port greater than 11.

## OTA updates

### Firmware

1. In ArduinoIDE run "Export compiled binary" of menu "Sketch".
1. Use the URL `/update` to load the update form titled `ElegantOTA`.
1. Ensure OTA mode is `Firmware`.
1. Select the bindary file exported.
   1. On MacOS it is found at /private/var/folders/b2/*/T/arduino/sketches/*/IrrigationManagementESP.ino.bin
   1. On Windows: t.b.d.
   1. On Linux: t.b.d.

### Webapp

1. Use the URL `/webapp-upload` to load the upload form.
1. Build the webapp by running `npm run build`
1. All files of the webapp have to be added for upload (`data/www/index.html` and all files in `data/www/assets`)!

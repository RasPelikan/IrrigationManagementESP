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

For water level, five [horizontal side-mount float switches](https://amzn.eu/d/0hU5uFkc) are used. Each switch is a sealed
magnetic reed contact that closes when the float lever is lifted by the water. Wiring uses the ESP32 internal pullup, so
each switch only needs two leads (signal + common GND) — pin pulled LOW = float up = wet, pin floats HIGH via pullup =
float down = dry. The firmware picks the highest closed switch as the current bracket (EMPTY / 0–25 / 25–50 / 50–75 /
75–100 / FULL).

To avoid drilling the IBC tank itself, the switches are mounted on an external **DN90 PVC standpipe** (≈1 m) connected
to the IBC outlet via a T-fitting and flex hose. Both vessels share the same water level (communicating vessels). The
standpipe is fixed to the IBC cage with 3D-printed clamps and rests on a 3D-printed base cup that bears the ~7 kg weight
when full. Cable splices are made dry inside an IP55 junction box mounted on the outside of the standpipe with WAGO 221
terminals; only the ~30 cm of factory cable from each float sits inside the standpipe.

## Build

There are two build paths: ArduinoIDE (interactive) and a headless shell-script
build (`build-firmware.sh`) designed for long-term reproducibility.

### Headless build (recommended for OTA updates)

The shell-script build pins all libraries via git submodules and the ESP32 core
via `setup-toolchain.sh`. It does not depend on the ArduinoIDE Library Manager.

```shell
git submodule update --init --recursive   # only the first time after clone
./setup-toolchain.sh                       # one-time: installs arduino-cli + ESP32 core
./build-firmware.sh                        # produces build/IrrigationManagementESP.ino.bin
./build-firmware.sh --with-webapp          # also rebuilds the webapp
```

Pinned versions are recorded in [`.versions.txt`](./.versions.txt). The toolchain
is installed into `~/Library/Arduino15-IrrigationManagementESP/` — outside the
sketch directory but project-specific (see [Gotchas](#gotchas) for why), so your
global ArduinoIDE installation is untouched.

#### Cold-storage rebuild

If you need to rebuild years from now and upstream sources are gone:

1. Restore the project from your own clone (the submodule contents are stored
   inside `.git/modules/` of any existing clone, so any old laptop with the repo
   has all library code).
2. Restore the ESP32 core from a tarball you previously created with
   `./setup-toolchain.sh --snapshot` (saves to `vendor/esp32-core-3.2.1.tar.gz`).
   Keep that tarball on an external drive or a GitHub release attached to this
   repo.
3. Run `./setup-toolchain.sh --from-vendor && ./build-firmware.sh`.

#### Gotchas

ArduinoIDE 2.x scans the sketch directory **recursively, including hidden
folders (those starting with `.`)** and validates every source file name
against Arduino sketch naming rules (`[A-Za-z0-9_.-]`, max 63 chars). A single
illegal name anywhere in the tree prevents the sketch from opening. The
project is structured around this:

- **Toolchain (`~/Library/Arduino15-IrrigationManagementESP/`)** lives outside
  the sketch directory. The ESP8266 BearSSL SDK ships e.g. `chain-ec+rsa.h`
  and the GCC toolchain has binaries named `c++`, `g++` — putting the core
  inside the sketch tree breaks the IDE.
- **Library submodules (`.libraries/`)** are dot-prefixed on purpose. Without
  the dot, `arduino-cli` automatically adds `<sketch>/libraries/` as a library
  search path, which collides with the user's global Library Manager
  installations and produces ambiguous-include errors. The dot also keeps the
  IDE's Explorer view uncluttered.
- **`build/`** is gitignored and ignored by the IDE.

If you ever see ArduinoIDE refuse to open the sketch with an error about a
file name, the first thing to check is `find . -name '*[+!@#$%^&]*'` and
`find . -name '*' | awk -F/ '{ if (length($NF) > 63) print }'` to locate the
offending file.

### ArduinoIDE build (interactive)

#### Board settings

- **Board:** `ESP32 Dev Module`
- **Flash Size:** 4MB
- **Partition Scheme:** Default 4MB with spiffs
- **Upload Speed:** 921600

If the ESP32 board package is not yet installed:
1. Arduino IDE → Preferences → Additional Board Manager URLs:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Tools → Board Manager → search "esp32" → install **esp32 by Espressif Systems**

#### Required libraries (Library Manager)

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

#### Progressive Web App (install on phone)

The webapp is set up as a PWA so it can be added to a phone's home screen and
launched fullscreen like a native app. Files involved (all in `webapp/public/`,
copied verbatim to `data/www/` by Vite):

| File | Purpose |
|------|---------|
| `manifest.json` | Web App Manifest (name, icons, theme colour, `display: standalone`). Named `.json` instead of `.webmanifest` because ESPAsyncWebServer's MIME table knows `.json` but not `.webmanifest`. |
| `sw.js` | Minimal service worker with a no-op `fetch` listener — exists only to satisfy Chrome's installability criteria; no asset pre-caching. |
| `icon.svg` | Master icon (water droplet with leaf). Used by Chrome and as favicon. |
| `icon-192.png`, `icon-512.png` | Pre-rendered PNGs for iOS `apple-touch-icon` and Android adaptive (`maskable`) icons. Regenerate from the SVG with `qlmanage -t -s 512 -o webapp/public webapp/public/icon.svg` (and `-s 192`) if the icon changes. |

Service workers require a secure context — installation on iOS works over plain
HTTP, but the full Android Chrome install prompt only appears when the device
is reached via HTTPS or `localhost`. The SW registration silently no-ops on
HTTP, so the site keeps working as a normal web page.

### Initial upload

Initially, the firmware and the files (webapp and config files) have to be uploaded via USB.
Once the device is available via HTTP over the air (OTA) updates of the firmware can be done via
URL `/update`. The configuration can be modified and also the webapp itself can be updated
via webapp.

#### Headless USB flash (recommended)

After running `./build-firmware.sh --with-webapp` and creating `data/credentials.json`
and `data/config.json` (see [Configuration files](#configuration-files)), provision a
fresh ESP32 over USB with `flash-new-esp.sh`:

```shell
./flash-new-esp.sh                          # auto-detects the serial port
./flash-new-esp.sh --port /dev/cu.usbserial-0001
./flash-new-esp.sh --erase                  # wipe the whole flash first
./flash-new-esp.sh --skip-fs                # firmware only
./flash-new-esp.sh --skip-firmware          # LittleFS only
```

The script does both steps in a single `esptool` invocation, flashing at the
standard ESP32 offsets (matches what the Arduino IDE writes over USB):
1. `bootloader.bin` at `0x1000`, `partitions.bin` at `0x8000`, `boot_app0.bin`
   at `0xe000`, app `.ino.bin` at `0x10000`.
2. LittleFS image built from `data/` recursively with `mklittlefs`, written to
   the LittleFS ("spiffs") partition.

The LittleFS partition offset and size are read at runtime from the ESP32 core's
`default.csv`, so a future core upgrade that changes the partition layout is
picked up automatically. Both `esptool` and `mklittlefs` come from the ESP32
core installed by `setup-toolchain.sh` — no extra tools needed.

After provisioning, all subsequent updates go over HTTP: firmware via
ElegantOTA at `/update`, webapp via `/webapp-upload`, config via the webapp.

#### Alternative: Arduino IDE LittleFS plugin

Once created initial config files (see section [Configuration files](#configuration-files))
and the webapp, one can use the
[Arduino IDE ESP32 LittleFS Filesystem Uploader Plugin](https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/)
to send all files to your board (Serial console has to be closed during upload!).

### Serial monitor

To watch the ESP32's serial output in the terminal — analogous to `tail -f`
for the device — use `monitor-esp.sh`:

```shell
./monitor-esp.sh                                # auto-detect port, 115200 baud
./monitor-esp.sh --port /dev/cu.usbserial-0001
./monitor-esp.sh --baud 74880                   # ESP boot ROM speed
./monitor-esp.sh --once                         # don't auto-reconnect on disconnect
```

The script auto-detects the USB serial port and reconnects automatically when
the device comes back (e.g. after a re-flash or a power cycle). It prefers
[`tio`](https://github.com/tio/tio) if installed (`brew install tio`, cleaner
UX, exit with Ctrl-T Q), and falls back to the system `screen` otherwise
(exit with Ctrl-A K, then y). Ctrl-C always quits the outer loop.

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
  "location": {             // optional - enables daylight-gated well pump in AUTO mode
    "latitude": 48.2082,    // decimal degrees, north positive
    "longitude": 16.3738    // decimal degrees, east positive
  },
  "pumps": {
    "well": {               // well-pump specific config
      "cycle": {            // the cycle in to pump water
                            // typically pumps are not meant to run without break
        "on": 45,           // minutes how long to pump
        "off": 15,          // minutes of the break
        "overfill": 300     // optional, seconds to keep pumping after FULL is reached
                            // (compensates for redistribution lag between connected containers).
                            // Omit or set to 0 to disable.
      },
      "daylight": {         // optional, only used when "location" is set
        "startOffsetMin": 60, // minutes after sunrise to allow pumping
        "endOffsetMin": 60    // minutes before sunset to stop pumping
      }
    },
    "irrigation": {         // irrigation specific config
      "hysteresis": 1200    // seconds how long to pause once the pump was switch off
                            // typically pumps are only allowed to start x times per hour
    }
  },
  "water": {
    "level": {              // water-level specific config
      "hysteresis": 15      // a falling water level must be reported consistently for this many
                            // consecutive seconds before it is committed. Rising readings are
                            // committed immediately. This prevents waves and bubbles from briefly
                            // dropping the reading to EMPTY (and aborting an active irrigation
                            // cycle). Pick a value that exceeds the typical wave-settle time but
                            // is short enough to still react quickly to a genuine empty container.
    },
    "pressure": {           // water-pressure specific config
      "low": 3.0,           // the low end in bar to start the irrigation pump
      "high": 5.5,          // the high end in bar to stop the irrigation pump
      "reference": {        // to setup ADC for pressure sensor
        "low": {            // pump water at pressure of e.g. 3 bar into the system (don't use 0 bar for 'low')
          "bar": 3.0,       // the exact pressure one can read from analog sensor
          "value": 350      // the ADC value shown in the webapp (values from 0 to 1023, 10-bit)
        },
        "high": {           // pump water at pressure of e.g. 8 bar into the system
          "bar": 8.0,       // the exact pressure one can read from analog sensor
          "value": 800      // the ADC value shown in the webapp (values from 0 to 1023, 10-bit)
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

1. Use the URL `/webapp-upload` to upload a new webapp.
1. Build the webapp by running `npm run build`:
   ```shell
   cd webapp
   npm install
   npm run build
1. All files of the webapp have to be added for upload (`data/www/index.html` and all files in `data/www/assets`)!
   Just add all files of all subdirectories into the upload form.

## Gallery

A look at the physical installation:

### System overview

![Annotated overview of the installation](readme/Overview.jpeg)

The complete rig: well pump pipe coming in from the top, water passing through a filter into the top opening of the IBC containers, three containers connected at the bottom so they share level, local irrigation valve, pressure tank, horizontally mounted submersible irrigation pump, and the digital pressure sensor for the pressure-based pump control.

### Storage — three connected IBC containers

![Three IBC containers connected at the bottom](readme/IBC-Containers.jpeg)

Three IBCs coupled at the bottom act as one tank with ~3 m³ capacity. Thanks to the bottom coupling the level reads the same.

### Water-level sensing — DN90 PVC standpipe

![DN90 PVC standpipe next to the IBC, connected at the bottom](readme/Waterlevel.jpeg)

External DN90 standpipe wired to the IBC outlet via a T-fitting and flex hose (communicating vessels). Five horizontal side-mount float switches are mounted along the pipe at the bracket positions. This avoids drilling the IBC walls and keeps the switches accessible for service.

### Controller — ESP32 + relays in a waterproof box

![Open controller enclosure with ESP32 and relay boards](readme/Controller.jpeg)

ESP32 (with external antenna) plus a relay board for the irrigation pump, well pump and two local irrigation valves. The white I/O switch on the right is the mains kill for manual lockout. Remote valves controlled over HTTP do not need a relay channel here — see the [client project](https://github.com/RasPelikan/IrrigationClientESP).

### Valves

![Two solenoid valves with quick-connect terminals](readme/Valves.jpeg)

230 V solenoid valves on the local outputs, driven via the relay board in the controller box.

### Irrigation pump — DIY housing build

A submersible deep-well pump is repurposed as the irrigation pump by mounting it horizontally inside a custom housing made from KG-Rohr (PVC sewer pipe) end caps. The five photos document the build:

| | |
|---|---|
| ![Pump test-fit in the pipe with 3D-printed alignment ring](readme/Mounting-the-pump-1.jpeg) | ![Outlet end with 3D-printed support ring](readme/Mounting-the-pump-2.jpeg) |
| ![Pump bottom with the suction screen visible inside the orange end cap](readme/Mounting-the-pump-3.jpeg) | ![Outlet cap with O-ring seat and threaded brass nipple](readme/Mounting-the-pump-4.jpeg) |
| ![Finished housing connected to the pressure-tank manifold](readme/Mounting-the-pump-5.jpeg) | |

### Webapp — Status page

![Webapp status page on a phone showing level, pump times, pressure, well/irrigation pump state, cycles and valves](readme/Screenshot.jpeg)

The Status page is the primary user interface, updated live via Server-Sent Events. Visible: current level (with the five raw float-switch indicators on the right), daylight pump window, pressure, well-pump and irrigation-pump state with mode toggles, cycles, all valves with per-valve mode toggles, and the firmware/webapp build provenance at the bottom.



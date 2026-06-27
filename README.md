# FluidNC-EspNow-Client

`FluidNC-EspNow-Client` is a PlatformIO library for building ESP32 peripherals that communicate with FluidNC in real-time over ESP-NOW.

## Features

- FluidNC-compatible ESP-NOW pairing labels and NVS namespace.
- Automatic `?` status polling.
- Full FluidNC/Grbl line-protocol parsing (GrblParser backend): status reports
  (up to 6 axes, overrides, pins, spindle/coolant, file progress), `ok`,
  `error:`, `ALARM:`, `[MSG:]`, `[PRB:]`, `[GC:]`, `[VER:]`, and coordinate-system
  offsets.
- Typed callbacks for status, errors, alarms, messages, probe results, gcode
  modes, versions, and offsets, plus pairing and connection events.
- `ready()`, `canSend()`, `isIdle()`, `isAlarmed()`, and `isJogging()` helpers.
- Newline-framed command helpers.
- Named realtime controls for status, feed hold, cycle start, soft reset, jog
  cancel, and feed/rapid/spindle overrides and coolant toggles.
- Convenience helpers for homing, unlock, safe macros, and safe jog commands.

See the [API docs](docs/api.md) for the full list of features: events, status fields,
commands, realtime controls, configuration, and limitations.

## Install In A PlatformIO Project

For a published release (PlatformIO pulls in the EspNowLink dependency
automatically):

```ini
lib_deps =
    figamore/FluidNC-EspNow-Client@^1.0.0
```

## Run The Examples

Each example is a standalone PlatformIO project:

```sh
pio run -d examples/BasicPendant
pio run -d examples/SerialBridge
```

- `examples/BasicPendant` (`src/main.cpp`) shows pairing, status printing, jogging
  with the arrow keys (and `+`/`-` for Z), and common realtime controls.
- `examples/SerialBridge` (`src/main.cpp`) forwards serial input to FluidNC and prints received
  status reports.

## Minimal Peripheral

```cpp
#include <Arduino.h>
#include <FluidNCEspNowClient.h>

FluidNCEspNowClient fluidnc("my-pendant");

void setup() {
  Serial.begin(115200);

  fluidnc.onStatus([](const FluidNCStatus& status) {
    Serial.printf("%s X%.3f Y%.3f Z%.3f\n",
                  status.state,
                  status.work.x,
                  status.work.y,
                  status.work.z);
  });

  fluidnc.onConnected([](const FluidNCMachine& machine) {
    Serial.printf("Connected to %s\n", machine.label().c_str());
  });

  if (!fluidnc.begin()) {
    Serial.println("FluidNC-EspNow-Client begin failed");
    return;
  }

  if (!fluidnc.isPaired()) {
    Serial.println("Open FluidNC's ESP-NOW pairing window by sending $espnow/pair command");
    fluidnc.startPairing();
  }
}

void loop() {
  fluidnc.poll();
}
```

## Machine Management

```cpp
fluidnc.startPairing();         // Pair with a FluidNC pairing window
fluidnc.machineCount();         // Number of remembered machines
fluidnc.getMachine(0, machine); // Hostname, MAC, channel, active flag
fluidnc.selectMachine(0);       // Switch remembered machine
fluidnc.forgetMachine(0);       // Forget one machine
fluidnc.forgetAllMachines();    // Forget every remembered machine
```

## License

FluidnC-EspNow-Client is licensed under the GNU General Public License v3.0 only. See `LICENSE`.
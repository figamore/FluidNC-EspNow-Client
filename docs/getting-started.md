# Getting Started

## Basic Model

- The FluidNC controller opens an ESP-NOW pairing window.
- The peripheral calls `startPairing()`.
- Once paired, credentials are stored in NVS and reconnect automatically.
- `poll()` drives the ESP-NOW transport and status polling.
- Status reports are parsed into `FluidNCStatus`.
- Typed callbacks cover the common UI events without requiring direct
  `EspNowLinkEvent` handling.

## Minimal Peripheral

```cpp
#include <Arduino.h>
#include <FluidNCEspNowClient.h>

FluidNCEspNowClient fluidnc("panel-1");

void setup() {
  Serial.begin(115200);

  fluidnc.onStatus([](const FluidNCStatus& status) {
    if (status.work.valid) {
      Serial.printf("X%.3f Y%.3f Z%.3f\n",
                    status.work.x,
                    status.work.y,
                    status.work.z);
    }
  });

  fluidnc.onConnected([](const FluidNCMachine& machine) {
    Serial.printf("Connected to %s\n", machine.label().c_str());
  });

  fluidnc.begin();
  if (!fluidnc.isPaired()) {
    fluidnc.startPairing();
  }
}

void loop() {
  fluidnc.poll();
}
```

!!! tip "Open the pairing window first"
    Pairing only succeeds while FluidNC's ESP-NOW pairing window is open. Send
    the `$espnow/pair` command to FluidNC before (or right after) calling
    `startPairing()`.

## Configuration

```cpp
FluidNCEspNowClientConfig cfg;
cfg.hostname = "shop-pendant";
cfg.autoStatusPoll = true;
cfg.statusPollIntervalMs = 250;

fluidnc.begin(cfg);
```

The default NVS namespace is `fluidespnow`, and the default protocol labels are
the FluidNC-compatible ESP-NOW labels. Override those only for a deliberate
compatibility break.

## Run The Examples

Each example is a standalone PlatformIO project:

```sh
pio run -d examples/BasicPendant
pio run -d examples/SerialBridge
```

- `examples/BasicPendant` (`src/main.cpp`) shows pairing, status printing, jogging
  with the arrow keys (and `+`/`-` for Z), and common realtime controls.
- `examples/SerialBridge` (`src/main.cpp`) forwards serial input to FluidNC and
  prints received status reports.

Next: [Pairing & Machines](pairing.md).

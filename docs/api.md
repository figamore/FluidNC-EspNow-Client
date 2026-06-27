# FluidNC-EspNow-Client Usage Guide

FluidNC-EspNow-Client is the FluidNC-shaped layer above EspNowLink. Use this library in
pendants, DROs, macro pads, probes, button panels, and other ESP32 peripherals
that pair with a FluidNC ESP-NOW server.

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

## Pairing And Machines

Use the machine-oriented helpers for normal user interfaces:

```cpp
fluidnc.startPairing();         // Start pairing with an open FluidNC pairing window
fluidnc.cancelPairing();        // Cancel pairing
fluidnc.machineCount();         // Remembered FluidNC machines
fluidnc.hasMachines();          // True when at least one machine is stored
fluidnc.activeMachineIndex();   // Active remembered machine, or -1
```

List remembered machines:

```cpp
for (size_t i = 0; i < fluidnc.machineCount(); ++i) {
  FluidNCMachine machine;
  if (fluidnc.getMachine(i, machine)) {
    Serial.println(machine.label());
  }
}
```

Switch or forget machines:

```cpp
fluidnc.selectMachine(0);
fluidnc.forgetMachine(0);
fluidnc.forgetActiveMachine();
fluidnc.forgetAllMachines();
```

## Events

Use typed callbacks for normal app code:

```cpp
fluidnc.onPairingStarted([]() {});
fluidnc.onPairingCancelled([]() {});
fluidnc.onPairingFailed([]() {});
fluidnc.onPaired([](const FluidNCMachine& machine) {});
fluidnc.onConnected([](const FluidNCMachine& machine) {});
fluidnc.onDisconnected([](const FluidNCMachine& machine) {});
fluidnc.onMachineChanged([]() {});
fluidnc.onSendFailed([]() {});
```

`onEvent()` is still available for advanced code that needs raw EspNowLink
details.

## FluidNC Protocol Events

Incoming FluidNC lines are parsed by the built-in GrblParser backend and surfaced
as typed callbacks. Register only the ones you need:

```cpp
fluidnc.onOk([]() {});                                   // a line was acknowledged
fluidnc.onError([](int code, const char* description) {  // error:N
  Serial.printf("error %d: %s\n", code, description);
});
fluidnc.onAlarm([](int code, const char* name) {         // ALARM:N
  Serial.printf("alarm %d: %s\n", code, name);
});
fluidnc.onMessage([](const char* category, const char* text) {  // [MSG:...]
  Serial.printf("[%s] %s\n", category, text);
});
fluidnc.onProbe([](const FluidNCProbeResult& probe) {    // [PRB:...]
  Serial.printf("probe %s at Z%.3f\n", probe.success ? "hit" : "miss", probe.position.z);
});
fluidnc.onGcodeModes([](const FluidNCGcodeModes& modes) {  // [GC:...]
  Serial.printf("units=%s wcs=%s\n", modes.units, modes.wcs);
});
fluidnc.onVersion([](const char* grbl, const char* fluidnc) {});  // [VER:...]
fluidnc.onOffset([](const char* name, const FluidNCPosition& offset) {});  // [G54:...]
```

`error:` codes and `ALARM:` codes also have static lookups:

```cpp
const char* text = FluidNCEspNowClient::errorDescription(code);
const char* name = FluidNCEspNowClient::alarmName(code);
```

## State Helpers

```cpp
fluidnc.canSend();   // Connected to a remembered machine
fluidnc.ready();     // Connected, has status, and FluidNC reports Idle
fluidnc.isIdle();
fluidnc.isAlarmed();
fluidnc.isJogging();
```

## Commands

Normal commands are sent as newline-terminated lines:

```cpp
fluidnc.sendLine("$H");
fluidnc.sendCommand("$H");
fluidnc.unlock();
fluidnc.home();
fluidnc.jog('X', 1.0f, 600.0f);
```

Prefer guarded helpers when user input could otherwise move the machine at the
wrong time:

```cpp
fluidnc.runMacro("G0 X0 Y0");              // Requires Idle
fluidnc.safeJog('X', 1.0f, 600.0f);        // Requires Idle
fluidnc.safeJogContinuous('Y', -1, 900.0f); // Requires Idle
```

Realtime controls are sent as realtime ESP-NOW packets:

```cpp
fluidnc.requestStatus();
fluidnc.feedHold();
fluidnc.cycleStart();
fluidnc.jogCancel();
fluidnc.softReset();
```

Feed, rapid, and spindle overrides plus coolant toggles are also realtime:

```cpp
fluidnc.feedOverrideReset();    // back to 100%
fluidnc.feedOverrideUp();       // +10%
fluidnc.feedOverrideDown();     // -10%
fluidnc.rapidOverrideReset();   // 100%
fluidnc.rapidOverrideMedium();  // 50%
fluidnc.rapidOverrideLow();     // 25%
fluidnc.spindleOverrideReset();
fluidnc.spindleOverrideUp();
fluidnc.spindleOverrideDown();
fluidnc.spindleStopToggle();
fluidnc.floodToggle();
fluidnc.mistToggle();
```

## Status Reports

Register `onStatus()` to receive parsed FluidNC status reports:

```cpp
fluidnc.onStatus([](const FluidNCStatus& status) {
  Serial.println(status.state);
});
```

`onStatus()` fires on every status report (about four per second by default), so
change-detect or throttle in the handler if you only act on changes.

`onStateChanged()` fires only when the machine state string changes (for example
`Idle`→`Run`→`Alarm`), which is handy for reacting to transitions without diffing
reports yourself:

```cpp
fluidnc.onStateChanged([](const char* state) {
  Serial.printf("state: %s\n", state);
});
```

Register `onLine()` if you also want every complete text line received from
FluidNC:

```cpp
fluidnc.onLine([](const char* line) {
  Serial.println(line);
});
```

`latestStatus()` returns the most recently parsed report, and `hasStatus()`
reports whether any valid status report has been received.

`FluidNCStatus` carries the full status report. FluidNC omits some fields when
they are unchanged, so `wco` and the overrides persist across updates. Per-report
fields - `limits`, `probePin`, and `controlPins` - always reflect the latest
report and clear when their field is absent:

```cpp
struct FluidNCStatus {
  char  state[16];           // "Idle", "Run", "Alarm", "Jog", ...
  FluidNCPosition work;      // x, y, z, a, b, c, valid
  FluidNCPosition machine;
  FluidNCPosition wco;
  size_t axisCount;

  bool  inch;                // from the last [GC:] units mode

  float feedRate;
  float spindleRpm;
  bool  feedSpindleValid;

  uint8_t feedOverride;      // percent
  uint8_t rapidOverride;
  uint8_t spindleOverride;
  bool    overridesValid;

  int   spindleState;        // 0 off, 1 CW, 2 CCW
  bool  flood;
  bool  mist;
  bool  accessoriesValid;

  bool  limits[6];           // per-axis limit pins
  bool  probePin;
  char  controlPins[24];     // any non-limit control pins, e.g. "D"

  int   lineNumber;
  bool  hasLineNumber;

  char  fileName[64];        // active SD job, if any
  int   filePercent;
  bool  fileActive;
};
```

`FluidNCPosition::axis(i)` returns axis `i` (0=X .. 5=C) for axis-count-agnostic
code.

You can feed raw FluidNC output through the same parser from any source (for
example a UART bridge) with `ingest()`:

```cpp
const char* report = "<Idle|MPos:0.000,0.000,0.000|FS:0,0>\n";
fluidnc.ingest(reinterpret_cast<const uint8_t*>(report), strlen(report));
```

## Advanced Access

Use `link()` when you need the underlying EspNowLink API:

```cpp
EspNowLink& radio = fluidnc.link();
Serial.println(radio.stateName());
```

## Notes And Limitations

- The parser backend uses shared state, so only one `FluidNCEspNowClient` instance can
  receive and parse FluidNC traffic at a time. One instance per peripheral is the
  intended usage. Switching the active machine (`selectMachine()`), disconnecting,
  or forgetting a machine clears the parsed state so reports never blend across
  machines.
- The bundled GrblParser exposes C functions such as `fnc_send_line()` and
  `fnc_realtime()`. These are internal plumbing - always use the `FluidNCEspNowClient`
  methods (`sendLine()`, `realtime()`, the override helpers, ...), which respect
  connection state and command guards.
- For press-and-hold jog buttons, track your own "jogging" flag when you send the
  jog and clear it on release. `isJogging()` reflects the last polled status, so it
  lags a button press by up to one status interval.

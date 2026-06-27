# Status Reports

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

## The `FluidNCStatus` Struct

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

## Ingesting Raw Output

You can feed raw FluidNC output through the same parser from any source (for
example a UART bridge) with `ingest()`:

```cpp
const char* report = "<Idle|MPos:0.000,0.000,0.000|FS:0,0>\n";
fluidnc.ingest(reinterpret_cast<const uint8_t*>(report), strlen(report));
```

Next: [Advanced & Limitations](advanced.md).

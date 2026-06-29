# FluidNC-EspNow-Client

`FluidNC-EspNow-Client` is a PlatformIO library for building ESP32 peripherals
that communicate with [FluidNC](https://github.com/bdring/FluidNC) in real time over
ESP-NOW. It is a FluidNC-shaped layer above
[EspNowLink](https://github.com/figamore/EspNowLink) - use it in pendants, DROs,
macro pads, probes, button panels, and other ESP32 peripherals that pair with a
FluidNC ESP-NOW server.

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
- Convenience helpers for homing, unlock, safe macros, safe jog commands, and SD file browsing/running.

## Install

For a published release (PlatformIO pulls in the EspNowLink dependency
automatically), add this to your `platformio.ini`:

```ini
lib_deps =
    figamore/FluidNC-EspNow-Client@^1.0.0
```

## Where To Next

<div class="grid cards" markdown>

- :material-rocket-launch: **[Getting Started](getting-started.md)** - the basic
  model, a minimal peripheral, and configuration.
- :material-link-variant: **[Pairing & Machines](pairing.md)** - pair, list,
  switch, and forget remembered FluidNC controllers.
- :material-bell-ring: **[Events](events.md)** - typed app and FluidNC
  protocol callbacks.
- :material-console-line: **[Commands & Realtime](commands.md)** - line commands,
  guarded helpers, and realtime controls.
- :material-chart-line: **[Status Reports](status.md)** - the parsed
  `FluidNCStatus` struct and how it updates.
- :material-cog: **[Advanced & Limitations](advanced.md)** - raw link access and
  things to know.

</div>

## License

`FluidNC-EspNow-Client` is licensed under the GNU General Public License v3.0
only. See [`LICENSE`](https://github.com/figamore/FluidNC-EspNow-Client/blob/main/LICENSE).

Next: [Getting Started](getting-started.md).

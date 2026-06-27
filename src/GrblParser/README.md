# Vendored GrblParser

These files are a vendored copy of GrblParser, which parses the Grbl/FluidNC
line protocol.

- Upstream: https://github.com/MitchBradley/GrblParser
- Author: Mitch Bradley, Copyright (c) 2023

## Local fixes

Upstream bugs are patched here, each marked with a `FluidNCEspNowClient local` comment
in `GrblParserC.c`.

- `collect()` bounds its writes to `REPORT_BUFFER_LEN` (upstream had no bound and
  could overflow on a long line or a newline-less stream).
- `parse_status_report()` initializes the feed/spindle pair and only reports it
  when `FS:` is present (upstream could publish uninitialized stack values).
- `parse_status_report()` clears the control-pin scratch once per report instead
  of per field (upstream cleared it mid-parse, so `Pn:` was dropped when other
  fields followed it).
- Probe-pin edge state persists across reports so it clears when the pin goes
  away (upstream compared against a per-call local that was always `false`).
- `parse_probe()` splits `[PRB:]` reports on `':'` (upstream used `'|'`, so
  probe reports were never parsed).
- `parse_status_report()` sets `has_linenum` for the `Ln:` field (upstream read
  the value but never reported it).
- `parse_gcode_report()` parses the `F` (feed) word from `[GC:]` (upstream left
  it commented out, so the gcode-modes feed was always zero).
- Added `fnc_parser_reset()` to clear remembered state (WCO, pins, gcode modes,
  partial line) on machine switch; WCO and probe-pin state moved to file scope.

FluidNC-EspNow-Client supplies the required `fnc_getchar` / `fnc_putchar` /
`milliseconds` symbols and overrides the parser's weak `show_*` callbacks. Do
not also link GrblParser separately in a project that uses FluidNC-EspNow-Client.

# Advanced & Limitations

## Advanced Access

Use `link()` when you need the underlying EspNowLink API:

```cpp
EspNowLink& radio = fluidnc.link();
Serial.println(radio.stateName());
```

## Notes And Limitations

- The parser backend uses shared state, so only one `FluidNCEspNowClient` instance
  can receive and parse FluidNC traffic at a time. One instance per peripheral is
  the intended usage. Switching the active machine (`selectMachine()`),
  disconnecting, or forgetting a machine clears the parsed state so reports never
  blend across machines.
- The bundled GrblParser exposes C functions such as `fnc_send_line()` and
  `fnc_realtime()`. These are internal plumbing - always use the
  `FluidNCEspNowClient` methods (`sendLine()`, `realtime()`, the override helpers,
  ...), which respect connection state and command guards.
- For press-and-hold jog buttons, track your own "jogging" flag when you send the
  jog and clear it on release. `isJogging()` reflects the last polled status, so it
  lags a button press by up to one status interval.

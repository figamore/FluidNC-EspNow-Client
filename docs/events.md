# Events

## App Events

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

Next: [Commands & Realtime](commands.md).

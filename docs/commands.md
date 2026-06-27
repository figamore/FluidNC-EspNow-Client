# Commands & Realtime

## State Helpers

```cpp
fluidnc.canSend();   // Connected to a remembered machine
fluidnc.ready();     // Connected, has status, and FluidNC reports Idle
fluidnc.isIdle();
fluidnc.isAlarmed();
fluidnc.isJogging();
```

## Line Commands

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

## Realtime Controls

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

Next: [Status Reports](status.md).

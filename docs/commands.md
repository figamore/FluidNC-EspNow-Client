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

## SD Files

FluidNC SD file lists are delivered as `[MSG:JSON]` chunks. The client handles
those chunks, acknowledges them, and exposes the parsed file list (sorted, files
before directories) through `onFileList()`. Directory entries report `size < 0`,
also available through `FluidNCFileInfo::isDirectory()`. The `truncated` callback
argument is retained for API compatibility and is always `false`.

```cpp
fluidnc.onFileList([](const char* path, const FluidNCFileInfo* files, size_t count, bool truncated) {
  Serial.printf("%s: %u entries%s\n", path, unsigned(count), truncated ? " (truncated)" : "");
  for (size_t i = 0; i < count; ++i) {
    Serial.printf("  %s%s\n", files[i].name, files[i].isDirectory() ? "/" : "");
  }
});

fluidnc.onFileListError([](const char* path, const char* status) {
  Serial.printf("file list failed for %s: %s\n", path, status);
});

fluidnc.requestFileList("/sd");
fluidnc.runFile("/sd/job.nc");      // Requires Idle
fluidnc.feedHold();                 // Pause running job
fluidnc.cycleStart();               // Resume held job
fluidnc.stopFile();                 // Abort the running SD job (realtime reset)
fluidnc.softReset();                // Emergency stop/reset
```

`requestFilePreview(path, firstLine, lineCount)` sends FluidNC's
`$File/ShowSome` command. The raw JSON reply is still available via `onMessage()`
for applications that want to render previews.

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

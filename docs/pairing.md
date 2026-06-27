# Pairing & Machines

Use the machine-oriented helpers for normal user interfaces:

```cpp
fluidnc.startPairing();         // Start pairing with an open FluidNC pairing window
fluidnc.cancelPairing();        // Cancel pairing
fluidnc.machineCount();         // Remembered FluidNC machines
fluidnc.hasMachines();          // True when at least one machine is stored
fluidnc.activeMachineIndex();   // Active remembered machine, or -1
```

## List Remembered Machines

```cpp
for (size_t i = 0; i < fluidnc.machineCount(); ++i) {
  FluidNCMachine machine;
  if (fluidnc.getMachine(i, machine)) {
    Serial.println(machine.label());
  }
}
```

## Switch Or Forget Machines

```cpp
fluidnc.selectMachine(0);
fluidnc.forgetMachine(0);
fluidnc.forgetActiveMachine();
fluidnc.forgetAllMachines();
```

!!! note
    Switching the active machine clears any parsed FluidNC state so reports never
    blend across machines. See [Advanced & Limitations](advanced.md) for details.

Next: [Events](events.md).

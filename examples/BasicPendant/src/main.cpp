// Copyright 2026 Figamore
// SPDX-License-Identifier: GPL-3.0-only

#include <Arduino.h>
#include <FluidNCEspNowClient.h>

FluidNCEspNowClient fluidnc("basic-pendant");

void printMachines() {
  const size_t count = fluidnc.machineCount();
  Serial.printf("Remembered machines: %u\n", unsigned(count));
  for (size_t i = 0; i < count; ++i) {
    FluidNCMachine machine;
    if (fluidnc.getMachine(i, machine)) {
      Serial.printf("  %u: %s%s\n",
                    unsigned(i),
                    machine.label().c_str(),
                    machine.active ? " *" : "");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  fluidnc.onPairingStarted([]() {
    Serial.println("Pairing started");
  });
  fluidnc.onPairingFailed([]() {
    Serial.println("Pairing failed");
  });
  fluidnc.onPaired([](const FluidNCMachine& machine) {
    Serial.printf("Paired with %s\n", machine.label().c_str());
  });
  fluidnc.onConnected([](const FluidNCMachine& machine) {
    Serial.printf("Connected to %s\n", machine.label().c_str());
  });
  fluidnc.onDisconnected([](const FluidNCMachine& machine) {
    Serial.printf("Disconnected from %s\n", machine.label().c_str());
  });
  fluidnc.onMachineChanged([]() {
    printMachines();
  });

  fluidnc.onStateChanged([](const char* state) {
    Serial.printf("state: %s\n", state);
  });

  fluidnc.onStatus([](const FluidNCStatus& status) {
    if (!status.work.valid) {
      return;
    }

    static bool seen = false;
    static float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
    if (seen && status.work.x == lastX && status.work.y == lastY && status.work.z == lastZ) {
      return;
    }
    seen = true;
    lastX = status.work.x;
    lastY = status.work.y;
    lastZ = status.work.z;

    Serial.printf("WPos X%.3f Y%.3f Z%.3f\n",
                  status.work.x,
                  status.work.y,
                  status.work.z);
  });

  fluidnc.onError([](int code, const char* description) {
    Serial.printf("error %d: %s\n", code, description);
  });
  fluidnc.onAlarm([](int code, const char* name) {
    Serial.printf("ALARM %d: %s\n", code, name);
  });
  fluidnc.onMessage([](const char* category, const char* text) {
    Serial.printf("[MSG:%s] %s\n", category, text);
  });

  if (!fluidnc.begin()) {
    Serial.println("FluidNCEspNowClient begin failed");
    return;
  }

  Serial.println("FluidNCEspNowClient basic pendant");
  Serial.println("Commands: p=pair m=list f=forget-active h=home u=unlock s=status !=hold ~=start r=reset c=jog-cancel");
  Serial.println("Jog (machine must be Idle): arrows = X/Y, +/- = Z. Fallback keys: x/y/z = +1mm, X/Y/Z = -1mm");

  if (!fluidnc.isPaired()) {
    Serial.println("Open FluidNC's ESP-NOW pairing window, then press p.");
  } else {
    printMachines();
  }
}

void doJog(char axis, float distanceMm, float feedrate) {
  if (!fluidnc.isIdle()) {
    const char* state = fluidnc.hasStatus() ? fluidnc.latestStatus().state : "unknown";
    Serial.printf("jog ignored: state %s (need Idle)\n", state);
    return;
  }
  if (fluidnc.safeJog(axis, distanceMm, feedrate)) {
    Serial.printf("jog %c%+.1f mm\n", axis, distanceMm);
  } else {
    Serial.println("jog send failed");
  }
}

// Arrow keys arrive as an escape sequence: ESC [ A/B/C/D (or ESC O A/.. on some
// terminals). Read the two bytes that follow an ESC and return the final letter.
int readEscapeFinal() {
  uint32_t start = millis();
  int b1;
  while ((b1 = Serial.read()) < 0) {
    if (millis() - start > 20) return -1;
  }
  if (b1 != '[' && b1 != 'O') return -1;
  start = millis();
  int b2;
  while ((b2 = Serial.read()) < 0) {
    if (millis() - start > 20) return -1;
  }
  return b2;
}

void loop() {
  fluidnc.poll();

  if (!Serial.available()) {
    return;
  }

  int ch = Serial.read();
  if (ch == 27) {
    switch (readEscapeFinal()) {
      case 'A': doJog('Y', 1.0f, 600.0f); break;   // up
      case 'B': doJog('Y', -1.0f, 600.0f); break;  // down
      case 'C': doJog('X', 1.0f, 600.0f); break;   // right
      case 'D': doJog('X', -1.0f, 600.0f); break;  // left
      default: break;
    }
    return;
  }

  switch (ch) {
    case 'p':
      fluidnc.startPairing();
      break;
    case 'm':
      printMachines();
      break;
    case 'f':
      fluidnc.forgetActiveMachine();
      printMachines();
      break;
    case 'h':
      fluidnc.home();
      break;
    case 'u':
      fluidnc.unlock();
      break;
    case 's':
      fluidnc.requestStatus();
      break;
    case '!':
      fluidnc.feedHold();
      break;
    case '~':
      fluidnc.cycleStart();
      break;
    case 'c':
      fluidnc.jogCancel();
      break;
    case 'r':
      fluidnc.softReset();
      break;
    case 'x':
      doJog('X', 1.0f, 600.0f);
      break;
    case 'X':
      doJog('X', -1.0f, 600.0f);
      break;
    case 'y':
      doJog('Y', 1.0f, 600.0f);
      break;
    case 'Y':
      doJog('Y', -1.0f, 600.0f);
      break;
    case 'z':
    case '+':
    case '=':
      doJog('Z', 1.0f, 300.0f);
      break;
    case 'Z':
    case '-':
      doJog('Z', -1.0f, 300.0f);
      break;
    default:
      break;
  }
}

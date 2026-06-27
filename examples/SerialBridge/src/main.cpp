// Copyright 2026 Figamore
// SPDX-License-Identifier: GPL-3.0-only

#include <Arduino.h>
#include <FluidNCEspNowClient.h>

FluidNCEspNowClient fluidnc("fluidnc-serial-bridge");

char commandLine[96] = {};
size_t commandLength = 0;

void sendBufferedCommand() {
  commandLine[commandLength] = '\0';
  if (commandLength > 0) {
    fluidnc.sendLine(commandLine);
  }
  commandLength = 0;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  fluidnc.onPairingStarted([]() {
    Serial.println("[pairing] started");
  });
  fluidnc.onPairingFailed([]() {
    Serial.println("[pairing] failed");
  });
  fluidnc.onConnected([](const FluidNCMachine& machine) {
    Serial.printf("[connected] %s\n", machine.label().c_str());
  });
  fluidnc.onDisconnected([](const FluidNCMachine& machine) {
    Serial.printf("[disconnected] %s\n", machine.label().c_str());
  });

  fluidnc.onLine([](const char* line) {
    Serial.println(line);
  });

  FluidNCEspNowClientConfig cfg;
  cfg.hostname = "fluidnc-serial-bridge";
  cfg.autoStatusPoll = false;

  if (!fluidnc.begin(cfg)) {
    Serial.println("FluidNCEspNowClient begin failed");
    return;
  }

  Serial.println("Serial bridge ready.");
  Serial.println("Commands: @pair @machines @forget-active @status @hold @start @cancel, or normal FluidNC lines like $H.");
}

void loop() {
  fluidnc.poll();

  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      commandLine[commandLength] = '\0';
      if (strcmp(commandLine, "@pair") == 0) {
        fluidnc.startPairing();
      } else if (strcmp(commandLine, "@machines") == 0) {
        for (size_t i = 0; i < fluidnc.machineCount(); ++i) {
          FluidNCMachine machine;
          if (fluidnc.getMachine(i, machine)) {
            Serial.printf("%u: %s\n", unsigned(i), machine.label().c_str());
          }
        }
      } else if (strcmp(commandLine, "@forget-active") == 0) {
        fluidnc.forgetActiveMachine();
      } else if (strcmp(commandLine, "@status") == 0) {
        fluidnc.requestStatus();
      } else if (strcmp(commandLine, "@hold") == 0) {
        fluidnc.feedHold();
      } else if (strcmp(commandLine, "@start") == 0) {
        fluidnc.cycleStart();
      } else if (strcmp(commandLine, "@cancel") == 0) {
        fluidnc.jogCancel();
      } else {
        sendBufferedCommand();
      }
      commandLength = 0;
    } else if (commandLength + 1 < sizeof(commandLine)) {
      commandLine[commandLength++] = c;
    } else {
      commandLength = 0;
      Serial.println("[error] command too long");
    }
  }
}

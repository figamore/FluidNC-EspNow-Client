// Copyright 2026 Figamore
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <Arduino.h>
#include <EspNowLink.h>

#include <functional>

namespace fluidnc_detail {
struct ParserBridge;
}

struct FluidNCPosition {
  static constexpr size_t kMaxAxes = 6;

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float a = 0.0f;
  float b = 0.0f;
  float c = 0.0f;
  bool valid = false;

  float axis(size_t index) const;
};

struct FluidNCStatus {
  char state[16] = {};
  FluidNCPosition work;
  FluidNCPosition machine;
  FluidNCPosition wco;
  size_t axisCount = 0;

  bool inch = false;

  float feedRate = 0.0f;
  float spindleRpm = 0.0f;
  bool feedSpindleValid = false;

  uint8_t feedOverride = 100;
  uint8_t rapidOverride = 100;
  uint8_t spindleOverride = 100;
  bool overridesValid = false;

  int spindleState = 0;  // 0 off, 1 CW, 2 CCW
  bool flood = false;
  bool mist = false;
  bool accessoriesValid = false;

  bool limits[FluidNCPosition::kMaxAxes] = {};
  bool probePin = false;
  char controlPins[24] = {};

  int lineNumber = 0;
  bool hasLineNumber = false;

  char fileName[64] = {};
  int filePercent = 0;
  bool fileActive = false;
};

struct FluidNCProbeResult {
  FluidNCPosition position;
  bool success = false;
};

struct FluidNCGcodeModes {
  const char* modal = nullptr;
  const char* wcs = nullptr;
  const char* plane = nullptr;
  const char* units = nullptr;
  const char* distance = nullptr;
  const char* program = nullptr;
  const char* spindle = nullptr;
  const char* mist = nullptr;
  const char* flood = nullptr;
  const char* parking = nullptr;
  int tool = 0;
  uint32_t spindleSpeed = 0;
  int32_t feed = 0;
};

enum class FluidNCCommandMode : uint8_t {
  SendAlways,
  RequireConnected,
  RequireIdle,
};

struct FluidNCMachine {
  static constexpr size_t kHostnameSize = 32;

  uint8_t mac[6] = {};
  char hostname[kHostnameSize] = {};
  uint8_t channel = 0;
  bool active = false;
  int index = -1;

  String macString() const;
  String label() const;
};

struct FluidNCEspNowClientConfig {
  const char* hostname = "fluidnc-peripheral";
  const char* nvsNamespace = "fluidespnow";
  uint32_t statusPollIntervalMs = 250;
  bool autoStatusPoll = true;
  bool autoStartPairing = false;
  bool setWifiMode = true;
  bool disconnectStaOnBegin = true;
  bool disableWifiSleep = true;
};

class FluidNCEspNowClient {
 public:
  using SimpleCallback = std::function<void()>;
  using EventCallback = std::function<void(const EspNowLinkEventInfo&)>;
  using MachineCallback = std::function<void(const FluidNCMachine& machine)>;
  using LineCallback = std::function<void(const char* line)>;
  using StatusCallback = std::function<void(const FluidNCStatus& status)>;
  using StateCallback = std::function<void(const char* state)>;
  using CodeCallback = std::function<void(int code, const char* description)>;
  using MessageCallback = std::function<void(const char* category, const char* text)>;
  using ProbeCallback = std::function<void(const FluidNCProbeResult& probe)>;
  using GcodeModesCallback = std::function<void(const FluidNCGcodeModes& modes)>;
  using VersionCallback = std::function<void(const char* grblVersion, const char* fluidncVersion)>;
  using OffsetCallback = std::function<void(const char* name, const FluidNCPosition& offset)>;

  static constexpr const char* kPairingWindowLabel = "fluidnc-espnow-pairing-window-v1";
  static constexpr const char* kPairingSessionLabel = "fluidnc-espnow-pairing-session-v1";
  static constexpr const char* kPmkLabel = "fluiddial-espnow-pmk-v1";
  static constexpr const char* kDefaultNvsNamespace = "fluidespnow";
  static constexpr size_t kMaxMachines = EspNowLink::kMaxProfiles;

  FluidNCEspNowClient();
  explicit FluidNCEspNowClient(const char* hostname);
  ~FluidNCEspNowClient();

  bool begin();
  bool begin(const FluidNCEspNowClientConfig& config);
  void end();
  void poll();

  bool startPairing(uint32_t timeoutMs = 0);
  void cancelPairing();
  bool isPairing() const;
  bool isPaired() const;
  bool isConnected() const;
  bool ready() const;
  bool canSend() const;
  bool isIdle() const;
  bool isAlarmed() const;
  bool isJogging() const;
  const char* stateName() const;
  int8_t rssi() const;

  size_t machineCount() const;
  bool hasMachines() const;
  bool getMachine(size_t index, FluidNCMachine& out) const;
  bool getActiveMachine(FluidNCMachine& out) const;
  int activeMachineIndex() const;
  bool selectMachine(size_t index);
  bool forgetMachine(size_t index);
  bool forgetActiveMachine();
  void forgetAllMachines();

  bool sendLine(const char* line);
  bool sendLine(const String& line);
  bool sendCommand(const char* command, FluidNCCommandMode mode = FluidNCCommandMode::RequireConnected);
  bool sendCommand(const String& command, FluidNCCommandMode mode = FluidNCCommandMode::RequireConnected);
  bool runMacro(const char* command);
  bool runMacro(const String& command);
  bool requestStatus();
  bool feedHold();
  bool cycleStart();
  bool softReset();
  bool jogCancel();
  bool realtime(uint8_t command);
  bool home();
  bool unlock();
  bool resetAlarms();
  bool jog(char axis, float distanceMm, float feedrateMmPerMin);
  bool jogContinuous(char axis, int direction, float feedrateMmPerMin, float distanceMm = 5000.0f);
  bool safeJog(char axis, float distanceMm, float feedrateMmPerMin);
  bool safeJogContinuous(char axis, int direction, float feedrateMmPerMin, float distanceMm = 5000.0f);

  bool feedOverrideReset();
  bool feedOverrideUp();
  bool feedOverrideDown();
  bool rapidOverrideReset();
  bool rapidOverrideMedium();
  bool rapidOverrideLow();
  bool spindleOverrideReset();
  bool spindleOverrideUp();
  bool spindleOverrideDown();
  bool spindleStopToggle();
  bool floodToggle();
  bool mistToggle();

  bool hasStatus() const;
  const FluidNCStatus& latestStatus() const;

  void ingest(const uint8_t* data, size_t len);

  void onEvent(EventCallback cb);
  void onPairingStarted(SimpleCallback cb);
  void onPairingCancelled(SimpleCallback cb);
  void onPairingFailed(SimpleCallback cb);
  void onPaired(MachineCallback cb);
  void onConnected(MachineCallback cb);
  void onDisconnected(MachineCallback cb);
  void onMachineChanged(SimpleCallback cb);
  void onSendFailed(SimpleCallback cb);
  void onLine(LineCallback cb);
  void onStatus(StatusCallback cb);
  void onStateChanged(StateCallback cb);
  void onOk(SimpleCallback cb);
  void onError(CodeCallback cb);
  void onAlarm(CodeCallback cb);
  void onMessage(MessageCallback cb);
  void onProbe(ProbeCallback cb);
  void onGcodeModes(GcodeModesCallback cb);
  void onVersion(VersionCallback cb);
  void onOffset(OffsetCallback cb);

  EspNowLink& link();
  const EspNowLink& link() const;

  static const char* errorDescription(int code);
  static const char* alarmName(int code);

 private:
  friend struct fluidnc_detail::ParserBridge;

  static constexpr size_t kLineBufferSize = 192;

  bool profileToMachine(size_t index, const EspNowLinkProfile& profile, FluidNCMachine& out) const;
  bool eventMachine(const uint8_t mac[6], FluidNCMachine& out) const;
  void installCallbacks();
  void handleEvent(const EspNowLinkEventInfo& event);
  void handleReceive(const uint8_t* data, size_t len);
  void resetStatusState();

  EspNowLink link_;
  FluidNCEspNowClientConfig config_;
  EventCallback eventCallback_;
  SimpleCallback pairingStartedCallback_;
  SimpleCallback pairingCancelledCallback_;
  SimpleCallback pairingFailedCallback_;
  MachineCallback pairedCallback_;
  MachineCallback connectedCallback_;
  MachineCallback disconnectedCallback_;
  SimpleCallback machineChangedCallback_;
  SimpleCallback sendFailedCallback_;
  LineCallback lineCallback_;
  StatusCallback statusCallback_;
  StateCallback stateChangedCallback_;
  SimpleCallback okCallback_;
  CodeCallback errorCallback_;
  CodeCallback alarmCallback_;
  MessageCallback messageCallback_;
  ProbeCallback probeCallback_;
  GcodeModesCallback gcodeModesCallback_;
  VersionCallback versionCallback_;
  OffsetCallback offsetCallback_;
  FluidNCStatus latestStatus_;
  FluidNCStatus parseAccum_;
  bool hasStatus_ = false;
  bool callbacksInstalled_ = false;
  uint32_t lastStatusPollMs_ = 0;
  char lineBuffer_[kLineBufferSize] = {};
  size_t lineLength_ = 0;
};

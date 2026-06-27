// Copyright 2026 Figamore
// SPDX-License-Identifier: GPL-3.0-only

#include "FluidNCEspNowClient.h"

#include "GrblParser/GrblParserC.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
extern const char* error_description[];
extern const char* alarm_name[];
}

namespace {

constexpr int kErrorDescriptionCount = 41;
constexpr int kAlarmNameCount = 16;

FluidNCEspNowClient* g_activeParser = nullptr;

char normalizedAxis(char axis) {
  axis = static_cast<char>(toupper(static_cast<unsigned char>(axis)));
  return axis == 'X' || axis == 'Y' || axis == 'Z' || axis == 'A' || axis == 'B' || axis == 'C' ? axis : '\0';
}

bool statusEquals(const FluidNCStatus& status, const char* state) {
  return status.state[0] != '\0' && strcmp(status.state, state) == 0;
}

void copyAxes(FluidNCPosition& out, const pos_t* axes, size_t n) {
  float values[FluidNCPosition::kMaxAxes] = {};
  for (size_t i = 0; i < n && i < FluidNCPosition::kMaxAxes; ++i) {
    values[i] = static_cast<float>(axes[i]);
  }
  out.x = values[0];
  out.y = values[1];
  out.z = values[2];
  out.a = values[3];
  out.b = values[4];
  out.c = values[5];
  out.valid = true;
}

}  // namespace

struct fluidnc_detail::ParserBridge {
  static void beginStatus(FluidNCEspNowClient* self) {
    self->parseAccum_ = self->latestStatus_;
  }

  static void state(FluidNCEspNowClient* self, const char* value) {
    strncpy(self->parseAccum_.state, value, sizeof(self->parseAccum_.state) - 1);
    self->parseAccum_.state[sizeof(self->parseAccum_.state) - 1] = '\0';
  }

  static void dro(FluidNCEspNowClient* self, const pos_t* axes, const pos_t* wcos, bool isMpos, const bool* limits, size_t n) {
    FluidNCStatus& s = self->parseAccum_;
    s.axisCount = n;
    copyAxes(s.wco, wcos, n);

    pos_t derived[FluidNCPosition::kMaxAxes] = {};
    for (size_t i = 0; i < n && i < FluidNCPosition::kMaxAxes; ++i) {
      derived[i] = isMpos ? axes[i] - wcos[i] : axes[i] + wcos[i];
    }

    if (isMpos) {
      copyAxes(s.machine, axes, n);
      copyAxes(s.work, derived, n);
    } else {
      copyAxes(s.work, axes, n);
      copyAxes(s.machine, derived, n);
    }

    for (size_t i = 0; i < FluidNCPosition::kMaxAxes; ++i) {
      s.limits[i] = (i < n) ? limits[i] : false;
    }
  }

  static void feedSpindle(FluidNCEspNowClient* self, uint32_t feed, uint32_t spindle) {
    self->parseAccum_.feedRate = static_cast<float>(feed);
    self->parseAccum_.spindleRpm = static_cast<float>(spindle);
    self->parseAccum_.feedSpindleValid = true;
  }

  static void overrides(FluidNCEspNowClient* self, uint32_t feed, uint32_t rapid, uint32_t spindle) {
    self->parseAccum_.feedOverride = static_cast<uint8_t>(feed);
    self->parseAccum_.rapidOverride = static_cast<uint8_t>(rapid);
    self->parseAccum_.spindleOverride = static_cast<uint8_t>(spindle);
    self->parseAccum_.overridesValid = true;
  }

  static void spindleCoolant(FluidNCEspNowClient* self, int spindle, bool flood, bool mist) {
    self->parseAccum_.spindleState = spindle;
    self->parseAccum_.flood = flood;
    self->parseAccum_.mist = mist;
    self->parseAccum_.accessoriesValid = true;
  }

  static void probePin(FluidNCEspNowClient* self, bool on) {
    self->parseAccum_.probePin = on;
  }

  static void controlPins(FluidNCEspNowClient* self, const char* pins) {
    strncpy(self->parseAccum_.controlPins, pins, sizeof(self->parseAccum_.controlPins) - 1);
    self->parseAccum_.controlPins[sizeof(self->parseAccum_.controlPins) - 1] = '\0';
  }

  static void lineNum(FluidNCEspNowClient* self, int n) {
    self->parseAccum_.lineNumber = n;
    self->parseAccum_.hasLineNumber = true;
  }

  static void file(FluidNCEspNowClient* self, const char* name, int percent) {
    strncpy(self->parseAccum_.fileName, name, sizeof(self->parseAccum_.fileName) - 1);
    self->parseAccum_.fileName[sizeof(self->parseAccum_.fileName) - 1] = '\0';
    self->parseAccum_.filePercent = percent;
    self->parseAccum_.fileActive = true;
  }

  static void endStatus(FluidNCEspNowClient* self) {
    const bool stateChanged = strcmp(self->latestStatus_.state, self->parseAccum_.state) != 0;
    self->latestStatus_ = self->parseAccum_;
    self->hasStatus_ = true;
    if (self->statusCallback_) {
      self->statusCallback_(self->latestStatus_);
    }
    if (stateChanged && self->stateChangedCallback_) {
      self->stateChangedCallback_(self->latestStatus_.state);
    }
  }

  static void ok(FluidNCEspNowClient* self) {
    if (self->okCallback_) {
      self->okCallback_();
    }
  }

  static void error(FluidNCEspNowClient* self, int code) {
    if (self->errorCallback_) {
      self->errorCallback_(code, FluidNCEspNowClient::errorDescription(code));
    }
  }

  static void alarm(FluidNCEspNowClient* self, int code) {
    if (self->alarmCallback_) {
      self->alarmCallback_(code, FluidNCEspNowClient::alarmName(code));
    }
  }

  static void message(FluidNCEspNowClient* self, const char* category, const char* text) {
    if (self->messageCallback_) {
      self->messageCallback_(category ? category : "", text ? text : "");
    }
  }

  static void probe(FluidNCEspNowClient* self, const pos_t* axes, bool success, size_t n) {
    if (!self->probeCallback_) {
      return;
    }
    FluidNCProbeResult result;
    copyAxes(result.position, axes, n);
    result.success = success;
    self->probeCallback_(result);
  }

  static void gcodeModes(FluidNCEspNowClient* self, const struct gcode_modes* modes) {
    FluidNCGcodeModes out;
    out.modal = modes->modal;
    out.wcs = modes->wcs;
    out.plane = modes->plane;
    out.units = modes->units;
    out.distance = modes->distance;
    out.program = modes->program;
    out.spindle = modes->spindle;
    out.mist = modes->mist;
    out.flood = modes->flood;
    out.parking = modes->parking;
    out.tool = modes->tool;
    out.spindleSpeed = modes->spindle_speed;
    out.feed = modes->feed;

    self->latestStatus_.inch = out.units && strcmp(out.units, "In") == 0;
    if (self->gcodeModesCallback_) {
      self->gcodeModesCallback_(out);
    }
  }

  static void versions(FluidNCEspNowClient* self, const char* grbl, const char* fluidnc) {
    if (self->versionCallback_) {
      self->versionCallback_(grbl ? grbl : "", fluidnc ? fluidnc : "");
    }
  }

  static void offset(FluidNCEspNowClient* self, const char* name, const pos_t* axes, size_t n) {
    if (!self->offsetCallback_) {
      return;
    }
    FluidNCPosition pos;
    copyAxes(pos, axes, n);
    self->offsetCallback_(name, pos);
  }
};

extern "C" {

int fnc_getchar() {
  return -1;
}

void fnc_putchar(uint8_t ch) {
  if (g_activeParser && g_activeParser->isConnected()) {
    g_activeParser->link().write(ch);
  }
}

int milliseconds() {
  return static_cast<int>(millis());
}

void begin_status_report() {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::beginStatus(g_activeParser);
  }
}

void end_status_report() {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::endStatus(g_activeParser);
  }
}

void show_state(const char* state) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::state(g_activeParser, state);
  }
}

void show_dro(const pos_t* axes, const pos_t* wcos, bool isMpos, bool* limits, size_t n_axis) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::dro(g_activeParser, axes, wcos, isMpos, limits, n_axis);
  }
}

void show_feed_spindle(uint32_t feedrate, uint32_t spindle_speed) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::feedSpindle(g_activeParser, feedrate, spindle_speed);
  }
}

void show_overrides(override_percent_t feed_ovr, override_percent_t rapid_ovr, override_percent_t spindle_ovr) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::overrides(g_activeParser, feed_ovr, rapid_ovr, spindle_ovr);
  }
}

void show_spindle_coolant(int spindle, bool flood, bool mist) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::spindleCoolant(g_activeParser, spindle, flood, mist);
  }
}

void show_probe_pin(bool on) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::probePin(g_activeParser, on);
  }
}

void show_control_pins(const char* pins) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::controlPins(g_activeParser, pins);
  }
}

void show_linenum(int linenum) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::lineNum(g_activeParser, linenum);
  }
}

void show_file(const char* filename, file_percent_t percent) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::file(g_activeParser, filename, static_cast<int>(percent));
  }
}

void show_ok() {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::ok(g_activeParser);
  }
}

void show_error(int error) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::error(g_activeParser, error);
  }
}

void show_alarm(int alarm) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::alarm(g_activeParser, alarm);
  }
}

void handle_msg(char* command, char* arguments) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::message(g_activeParser, command, arguments);
  }
}

void show_probe(const pos_t* axes, const bool probe_success, size_t n_axis) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::probe(g_activeParser, axes, probe_success, n_axis);
  }
}

void show_gcode_modes(struct gcode_modes* modes) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::gcodeModes(g_activeParser, modes);
  }
}

void show_versions(const char* grbl_version, const char* fluidnc_version) {
  if (g_activeParser) {
    fluidnc_detail::ParserBridge::versions(g_activeParser, grbl_version, fluidnc_version);
  }
}

void show_offset(offset_t offset, const pos_t* axes, size_t n_axis) {
  if (!g_activeParser) {
    return;
  }
  const char* name = "G92";
  switch (offset) {
    case G54: name = "G54"; break;
    case G55: name = "G55"; break;
    case G56: name = "G56"; break;
    case G57: name = "G57"; break;
    case G58: name = "G58"; break;
    case G59: name = "G59"; break;
    case G28: name = "G28"; break;
    case G30: name = "G30"; break;
    case G92: name = "G92"; break;
  }
  fluidnc_detail::ParserBridge::offset(g_activeParser, name, axes, n_axis);
}

}  // extern "C"

float FluidNCPosition::axis(size_t index) const {
  switch (index) {
    case 0: return x;
    case 1: return y;
    case 2: return z;
    case 3: return a;
    case 4: return b;
    case 5: return c;
    default: return 0.0f;
  }
}

String FluidNCMachine::macString() const {
  return EspNowLink::formatMac(mac);
}

String FluidNCMachine::label() const {
  const char* name = hostname[0] ? hostname : "FluidNC";
  return String(name) + "  " + macString();
}

FluidNCEspNowClient::FluidNCEspNowClient() = default;

FluidNCEspNowClient::FluidNCEspNowClient(const char* hostname) {
  config_.hostname = hostname;
}

FluidNCEspNowClient::~FluidNCEspNowClient() {
  if (g_activeParser == this) {
    g_activeParser = nullptr;
  }
}

bool FluidNCEspNowClient::begin() {
  return begin(config_);
}

bool FluidNCEspNowClient::begin(const FluidNCEspNowClientConfig& config) {
  config_ = config;
  installCallbacks();
  g_activeParser = this;

  EspNowLinkConfig linkConfig;
  linkConfig.role = EspNowLinkRole::Client;
  linkConfig.hostname = config_.hostname && config_.hostname[0] ? config_.hostname : "fluidnc-peripheral";
  linkConfig.nvsNamespace =
      config_.nvsNamespace && config_.nvsNamespace[0] ? config_.nvsNamespace : kDefaultNvsNamespace;
  linkConfig.setWifiMode = config_.setWifiMode;
  linkConfig.disconnectStaOnClientBegin = config_.disconnectStaOnBegin;
  linkConfig.disableWifiSleep = config_.disableWifiSleep;
  linkConfig.labels.pairingWindow = kPairingWindowLabel;
  linkConfig.labels.pairingSession = kPairingSessionLabel;
  linkConfig.labels.pmk = kPmkLabel;

  if (!link_.begin(linkConfig)) {
    return false;
  }

  if (config_.autoStartPairing && !link_.isPaired()) {
    link_.startPairing();
  }

  return true;
}

void FluidNCEspNowClient::end() {
  link_.end();
  resetStatusState();
  lastStatusPollMs_ = 0;
  if (g_activeParser == this) {
    g_activeParser = nullptr;
  }
}

void FluidNCEspNowClient::poll() {
  link_.poll();

  if (!config_.autoStatusPoll || !link_.isConnected()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastStatusPollMs_ >= config_.statusPollIntervalMs) {
    lastStatusPollMs_ = now;
    requestStatus();
  }
}

bool FluidNCEspNowClient::startPairing(uint32_t timeoutMs) {
  return link_.startPairing(timeoutMs);
}

void FluidNCEspNowClient::cancelPairing() {
  link_.cancelPairing();
}

bool FluidNCEspNowClient::isPairing() const {
  return link_.isPairing();
}

bool FluidNCEspNowClient::isPaired() const {
  return link_.isPaired();
}

bool FluidNCEspNowClient::isConnected() const {
  return link_.isConnected();
}

bool FluidNCEspNowClient::ready() const {
  return isConnected() && hasStatus_ && isIdle();
}

bool FluidNCEspNowClient::canSend() const {
  return isConnected();
}

bool FluidNCEspNowClient::isIdle() const {
  return hasStatus_ && statusEquals(latestStatus_, "Idle");
}

bool FluidNCEspNowClient::isAlarmed() const {
  return hasStatus_ && statusEquals(latestStatus_, "Alarm");
}

bool FluidNCEspNowClient::isJogging() const {
  return hasStatus_ && statusEquals(latestStatus_, "Jog");
}

const char* FluidNCEspNowClient::stateName() const {
  return link_.stateName();
}

int8_t FluidNCEspNowClient::rssi() const {
  return link_.rssi();
}

size_t FluidNCEspNowClient::machineCount() const {
  return link_.profileCount();
}

bool FluidNCEspNowClient::hasMachines() const {
  return machineCount() > 0;
}

bool FluidNCEspNowClient::getMachine(size_t index, FluidNCMachine& out) const {
  EspNowLinkProfile profile;
  if (!link_.getProfile(index, profile)) {
    return false;
  }
  return profileToMachine(index, profile, out);
}

bool FluidNCEspNowClient::getActiveMachine(FluidNCMachine& out) const {
  const int index = activeMachineIndex();
  return index >= 0 && getMachine(static_cast<size_t>(index), out);
}

int FluidNCEspNowClient::activeMachineIndex() const {
  return link_.activeProfileIndex();
}

bool FluidNCEspNowClient::selectMachine(size_t index) {
  if (!link_.selectProfile(index)) {
    return false;
  }
  resetStatusState();
  return true;
}

bool FluidNCEspNowClient::forgetMachine(size_t index) {
  if (!link_.removeProfile(index)) {
    return false;
  }
  resetStatusState();
  return true;
}

bool FluidNCEspNowClient::forgetActiveMachine() {
  const int index = activeMachineIndex();
  return index >= 0 && forgetMachine(static_cast<size_t>(index));
}

void FluidNCEspNowClient::forgetAllMachines() {
  link_.clearProfiles();
  resetStatusState();
}

void FluidNCEspNowClient::resetStatusState() {
  latestStatus_ = {};
  parseAccum_ = {};
  hasStatus_ = false;
  lineLength_ = 0;
  fnc_parser_reset();
}

bool FluidNCEspNowClient::sendLine(const char* line) {
  if (!line || !link_.isConnected()) {
    return false;
  }

  const size_t len = strlen(line);
  if (len > 0 && link_.write(reinterpret_cast<const uint8_t*>(line), len) != len) {
    return false;
  }

  return link_.write('\n') == 1;
}

bool FluidNCEspNowClient::sendLine(const String& line) {
  return sendLine(line.c_str());
}

bool FluidNCEspNowClient::sendCommand(const char* command, FluidNCCommandMode mode) {
  if (!command) {
    return false;
  }
  if (mode == FluidNCCommandMode::RequireConnected && !canSend()) {
    return false;
  }
  if (mode == FluidNCCommandMode::RequireIdle && !isIdle()) {
    return false;
  }
  return sendLine(command);
}

bool FluidNCEspNowClient::sendCommand(const String& command, FluidNCCommandMode mode) {
  return sendCommand(command.c_str(), mode);
}

bool FluidNCEspNowClient::runMacro(const char* command) {
  return sendCommand(command, FluidNCCommandMode::RequireIdle);
}

bool FluidNCEspNowClient::runMacro(const String& command) {
  return runMacro(command.c_str());
}

bool FluidNCEspNowClient::requestStatus() {
  return realtime('?');
}

bool FluidNCEspNowClient::feedHold() {
  return realtime('!');
}

bool FluidNCEspNowClient::cycleStart() {
  return realtime('~');
}

bool FluidNCEspNowClient::softReset() {
  return realtime(0x18);
}

bool FluidNCEspNowClient::jogCancel() {
  return realtime(0x85);
}

bool FluidNCEspNowClient::realtime(uint8_t command) {
  return link_.isConnected() && link_.sendRealtime(command);
}

bool FluidNCEspNowClient::home() {
  return sendCommand("$H");
}

bool FluidNCEspNowClient::unlock() {
  return sendCommand("$X");
}

bool FluidNCEspNowClient::resetAlarms() {
  return unlock();
}

bool FluidNCEspNowClient::jog(char axis, float distanceMm, float feedrateMmPerMin) {
  axis = normalizedAxis(axis);
  if (!axis || !isfinite(distanceMm) || !isfinite(feedrateMmPerMin) || feedrateMmPerMin <= 0.0f) {
    return false;
  }

  char command[64];
  snprintf(command, sizeof(command), "$J=G91G21F%.3f%c%.3f", feedrateMmPerMin, axis, distanceMm);
  return sendLine(command);
}

bool FluidNCEspNowClient::jogContinuous(char axis, int direction, float feedrateMmPerMin, float distanceMm) {
  axis = normalizedAxis(axis);
  if (!axis || direction == 0 || !isfinite(feedrateMmPerMin) || feedrateMmPerMin <= 0.0f ||
      !isfinite(distanceMm) || distanceMm <= 0.0f) {
    return false;
  }

  const float signedDistance = direction < 0 ? -distanceMm : distanceMm;
  return jog(axis, signedDistance, feedrateMmPerMin);
}

bool FluidNCEspNowClient::safeJog(char axis, float distanceMm, float feedrateMmPerMin) {
  return isIdle() && jog(axis, distanceMm, feedrateMmPerMin);
}

bool FluidNCEspNowClient::safeJogContinuous(char axis, int direction, float feedrateMmPerMin, float distanceMm) {
  return isIdle() && jogContinuous(axis, direction, feedrateMmPerMin, distanceMm);
}

bool FluidNCEspNowClient::feedOverrideReset() {
  return realtime(FeedOvrReset);
}

bool FluidNCEspNowClient::feedOverrideUp() {
  return realtime(FeedOvrCoarsePlus);
}

bool FluidNCEspNowClient::feedOverrideDown() {
  return realtime(FeedOvrCoarseMinus);
}

bool FluidNCEspNowClient::rapidOverrideReset() {
  return realtime(RapidOvrReset);
}

bool FluidNCEspNowClient::rapidOverrideMedium() {
  return realtime(RapidOvrMedium);
}

bool FluidNCEspNowClient::rapidOverrideLow() {
  return realtime(RapidOvrLow);
}

bool FluidNCEspNowClient::spindleOverrideReset() {
  return realtime(SpindleOvrReset);
}

bool FluidNCEspNowClient::spindleOverrideUp() {
  return realtime(SpindleOvrCoarsePlus);
}

bool FluidNCEspNowClient::spindleOverrideDown() {
  return realtime(SpindleOvrCoarseMinus);
}

bool FluidNCEspNowClient::spindleStopToggle() {
  return realtime(SpindleOvrStop);
}

bool FluidNCEspNowClient::floodToggle() {
  return realtime(CoolantFloodOvrToggle);
}

bool FluidNCEspNowClient::mistToggle() {
  return realtime(CoolantMistOvrToggle);
}

bool FluidNCEspNowClient::hasStatus() const {
  return hasStatus_;
}

const FluidNCStatus& FluidNCEspNowClient::latestStatus() const {
  return latestStatus_;
}

void FluidNCEspNowClient::ingest(const uint8_t* data, size_t len) {
  if (!data) {
    return;
  }

  g_activeParser = this;
  for (size_t i = 0; i < len; ++i) {
    const uint8_t byte = data[i];
    collect(byte);

    const char c = static_cast<char>(byte);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      lineBuffer_[lineLength_] = '\0';
      if (lineLength_ > 0 && lineCallback_) {
        lineCallback_(lineBuffer_);
      }
      lineLength_ = 0;
    } else if (lineLength_ + 1 < sizeof(lineBuffer_)) {
      lineBuffer_[lineLength_++] = c;
    } else {
      lineLength_ = 0;
    }
  }
}

void FluidNCEspNowClient::onEvent(EventCallback cb) {
  eventCallback_ = cb;
}

void FluidNCEspNowClient::onPairingStarted(SimpleCallback cb) {
  pairingStartedCallback_ = cb;
}

void FluidNCEspNowClient::onPairingCancelled(SimpleCallback cb) {
  pairingCancelledCallback_ = cb;
}

void FluidNCEspNowClient::onPairingFailed(SimpleCallback cb) {
  pairingFailedCallback_ = cb;
}

void FluidNCEspNowClient::onPaired(MachineCallback cb) {
  pairedCallback_ = cb;
}

void FluidNCEspNowClient::onConnected(MachineCallback cb) {
  connectedCallback_ = cb;
}

void FluidNCEspNowClient::onDisconnected(MachineCallback cb) {
  disconnectedCallback_ = cb;
}

void FluidNCEspNowClient::onMachineChanged(SimpleCallback cb) {
  machineChangedCallback_ = cb;
}

void FluidNCEspNowClient::onSendFailed(SimpleCallback cb) {
  sendFailedCallback_ = cb;
}

void FluidNCEspNowClient::onLine(LineCallback cb) {
  lineCallback_ = cb;
}

void FluidNCEspNowClient::onStatus(StatusCallback cb) {
  statusCallback_ = cb;
}

void FluidNCEspNowClient::onStateChanged(StateCallback cb) {
  stateChangedCallback_ = cb;
}

void FluidNCEspNowClient::onOk(SimpleCallback cb) {
  okCallback_ = cb;
}

void FluidNCEspNowClient::onError(CodeCallback cb) {
  errorCallback_ = cb;
}

void FluidNCEspNowClient::onAlarm(CodeCallback cb) {
  alarmCallback_ = cb;
}

void FluidNCEspNowClient::onMessage(MessageCallback cb) {
  messageCallback_ = cb;
}

void FluidNCEspNowClient::onProbe(ProbeCallback cb) {
  probeCallback_ = cb;
}

void FluidNCEspNowClient::onGcodeModes(GcodeModesCallback cb) {
  gcodeModesCallback_ = cb;
}

void FluidNCEspNowClient::onVersion(VersionCallback cb) {
  versionCallback_ = cb;
}

void FluidNCEspNowClient::onOffset(OffsetCallback cb) {
  offsetCallback_ = cb;
}

EspNowLink& FluidNCEspNowClient::link() {
  return link_;
}

const EspNowLink& FluidNCEspNowClient::link() const {
  return link_;
}

const char* FluidNCEspNowClient::errorDescription(int code) {
  if (code >= 0 && code < kErrorDescriptionCount) {
    return error_description[code];
  }
  return "";
}

const char* FluidNCEspNowClient::alarmName(int code) {
  if (code >= 0 && code < kAlarmNameCount) {
    return alarm_name[code];
  }
  return "";
}

bool FluidNCEspNowClient::profileToMachine(size_t index, const EspNowLinkProfile& profile, FluidNCMachine& out) const {
  out = {};
  memcpy(out.mac, profile.mac, sizeof(out.mac));
  strncpy(out.hostname, profile.hostname, sizeof(out.hostname) - 1);
  out.hostname[sizeof(out.hostname) - 1] = '\0';
  out.channel = profile.channel;
  out.active = profile.active;
  out.index = static_cast<int>(index);
  return true;
}

bool FluidNCEspNowClient::eventMachine(const uint8_t mac[6], FluidNCMachine& out) const {
  if (mac) {
    for (size_t i = 0; i < machineCount(); ++i) {
      FluidNCMachine machine;
      if (getMachine(i, machine) && memcmp(machine.mac, mac, sizeof(machine.mac)) == 0) {
        out = machine;
        return true;
      }
    }
  }
  return getActiveMachine(out);
}

void FluidNCEspNowClient::installCallbacks() {
  if (callbacksInstalled_) {
    return;
  }

  link_.onEvent([this](const EspNowLinkEventInfo& event) {
    handleEvent(event);
  });
  link_.onReceive([this](const uint8_t* data, size_t len, const uint8_t*) {
    handleReceive(data, len);
  });
  callbacksInstalled_ = true;
}

void FluidNCEspNowClient::handleEvent(const EspNowLinkEventInfo& event) {
  if (eventCallback_) {
    eventCallback_(event);
  }

  FluidNCMachine machine;
  switch (event.type) {
    case EspNowLinkEvent::PairingStarted:
      if (pairingStartedCallback_) {
        pairingStartedCallback_();
      }
      break;
    case EspNowLinkEvent::PairingCancelled:
      if (pairingCancelledCallback_) {
        pairingCancelledCallback_();
      }
      break;
    case EspNowLinkEvent::PairingFailed:
      if (pairingFailedCallback_) {
        pairingFailedCallback_();
      }
      break;
    case EspNowLinkEvent::Paired:
      if (pairedCallback_ && eventMachine(event.mac, machine)) {
        pairedCallback_(machine);
      }
      break;
    case EspNowLinkEvent::Connected:
      if (connectedCallback_ && eventMachine(event.mac, machine)) {
        connectedCallback_(machine);
      }
      break;
    case EspNowLinkEvent::Disconnected:
      resetStatusState();
      if (disconnectedCallback_ && eventMachine(event.mac, machine)) {
        disconnectedCallback_(machine);
      }
      break;
    case EspNowLinkEvent::ProfileChanged:
      resetStatusState();
      if (machineChangedCallback_) {
        machineChangedCallback_();
      }
      break;
    case EspNowLinkEvent::SendFailed:
      if (sendFailedCallback_) {
        sendFailedCallback_();
      }
      break;
    default:
      break;
  }
}

void FluidNCEspNowClient::handleReceive(const uint8_t* data, size_t len) {
  ingest(data, len);
}

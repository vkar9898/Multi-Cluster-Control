// BMW F10 6WB on CAN1. Frames ported from the BMW F code in r00li's CarCluster
// (https://github.com/r00li/CarCluster, GPL-3.0), via the local F10Cluster rewrite.
#include "board.h"

static constexpr uint32_t FAST_MS = 100, SLOW_MS = 500, CC_REFRESH_MS = 1000;
static constexpr uint32_t DATE_MS = 1000, SLOW5S_MS = 5000;
static constexpr uint32_t IGN_CYCLE_MS = 1500;  // screen needs ign off -> on

enum : uint8_t { GEAR_CLEAR = 0, GEAR_P = 10, GEAR_R = 11, GEAR_N = 12, GEAR_D = 13 };
enum LampKind : uint8_t { KIND_CC, KIND_ALERT, KIND_PARK };
struct Lamp { const char *name; LampKind kind; uint8_t ccId; bool on; };
static Lamp lamps[] = {
  {"dscoff", KIND_CC, 36},    {"dscactive", KIND_ALERT, 215}, {"dsc35", KIND_CC, 35},
  {"parkbrake", KIND_PARK, 0}, {"parkfault", KIND_CC, 24},    {"cc71", KIND_CC, 71},
  {"cel", KIND_CC, 34},       {"overheat", KIND_CC, 39},      {"seatbelt", KIND_CC, 77},
  {"door", KIND_ALERT, 0x0F},
};

static bool running = false;
static int rpm = 0, speedKmh = 0, tempC = 100, fuelPct = 100, backlight = 100, driveMode = 2;
static uint8_t gear = GEAR_P;
static bool highBeam = false, mainLights = true, frontFog = false, rearFog = false;
static Turn turn;

static uint8_t counter = 0, count = 0, accCounter = 0, sosCounter = 0;
static uint16_t distance = 0;
static uint32_t ignOnAt = 0, lastFast = 0, lastSlow = 0, lastCc = 0, lastDate = 0, last5s = 0;

static uint8_t crc8(const uint8_t *data, uint8_t n, uint8_t finalXor) {
  uint8_t c = 0xFF;
  for (uint8_t i = 0; i < n; i++) {
    c ^= data[i];
    for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (c << 1) ^ 0x1D : c << 1;
  }
  return c ^ finalXor;
}

static void sendCrc(uint16_t id, const uint8_t *body, uint8_t n, uint8_t finalXor) {
  uint8_t f[8];
  f[0] = crc8(body, n, finalXor);
  memcpy(f + 1, body, n);
  can1.send(id, n + 1, f);
}

static uint8_t tick(uint8_t high) { return high | counter; }

static void sendIgnition(bool on) {
  const uint8_t b[] = {tick(0x80), (uint8_t)(on ? 0x8A : 0x08), 0xDD, 0xF1, 0x01, 0x30, 0x06};
  sendCrc(0x12F, b, 7, 0x44);
}

static void sendSpeed() {
  const uint16_t raw = speedKmh * 64.01;
  const uint8_t b[] = {tick(0xC0), (uint8_t)raw, (uint8_t)(raw >> 8), (uint8_t)(speedKmh == 0 ? 0x81 : 0x91)};
  sendCrc(0x1A1, b, 4, 0xA9);
}

static void sendRpm() {
  uint8_t g = 0;
  if (gear >= 1 && gear <= 9) g = gear + 4;
  else if (gear == GEAR_R) g = 2;
  else if (gear == GEAR_N) g = 1;
  const uint8_t b[] = {tick(0x60), (uint8_t)map(rpm, 0, 6900, 0x00, 0x2B), 0xC0, 0xF0, g, 0xFF, 0xFF};
  sendCrc(0x0F3, b, 7, 0x7A);
}

static void sendTransmission() {
  uint8_t sel = 0;
  if (gear >= 1 && gear <= 9) sel = 0x81;
  else if (gear == GEAR_P) sel = 0x20;
  else if (gear == GEAR_R) sel = 0x40;
  else if (gear == GEAR_N) sel = 0x60;
  else if (gear == GEAR_D) sel = 0x80;
  const uint8_t b[] = {counter, sel, 0xFC, 0xFF};
  sendCrc(0x3FD, b, 4, 0xD6);
}

static void sendBasicInfo() {
  const uint8_t abs1[] = {tick(0xF0), 0xFE, 0xFF, 0x14};
  sendCrc(0x36E, abs1, 4, 0xD8);
  const uint8_t alive[] = {count, 0xFF};
  can1.send(0x0D7, 2, alive);
  const uint8_t steer[] = {tick(0xF0), 0xFE, 0xFF, 0x14};
  sendCrc(0x2A7, steer, 4, 0x9E);
  const uint8_t cruise[] = {tick(0xF0), 0xE0, 0xE0, 0xE1, 0x00, 0xEC, 0x01};
  sendCrc(0x289, cruise, 7, 0x82);
  const uint8_t restraint[] = {tick(0x40), 0x40, 0x55, 0xFD, 0xFF, 0xFF, 0xFF};
  sendCrc(0x19B, restraint, 7, 0xFF);
  const uint8_t restraint2[] = {tick(0xE0), 0xF1, 0xF0, 0xF2, 0xF2, 0xFE};
  sendCrc(0x297, restraint2, 6, 0x28);
  const uint8_t tpms[] = {tick(0xF0), 0xA2, 0xA0, 0xA0};
  sendCrc(0x369, tpms, 4, 0xC5);
  const uint8_t oil[] = {tick(0x10), 0x82, 0x4E, 0x7E, (uint8_t)(tempC + 50), 0x05, 0x89};
  sendCrc(0x3F9, oil, 7, 0xF1);
  const uint8_t engine[] = {0x3E, (uint8_t)tempC, 0x64, 0x64, 0x64, 0x01, 0xF1};
  sendCrc(0x2C4, engine, 7, 0xB2);
}

static void sendFuel() {
  const uint8_t raw = fuelPct <= 50 ? map(fuelPct, 0, 50, 37, 18) : map(fuelPct, 50, 100, 18, 4);
  const uint8_t b[] = {0x00, raw, 0x00, raw, 0x00};
  can1.send(0x349, 5, b);
}

static bool lampOn(const char *name) {
  for (const Lamp &l : lamps) {
    if (!strcmp(l.name, name)) return l.on;
  }
  return false;
}

static void sendParkBrake() {
  const uint8_t b[] = {tick(0xF0), 0x38, 0x00, (uint8_t)(lampOn("parkbrake") ? 0x15 : 0x14)};
  sendCrc(0x36F, b, 4, 0x17);
}

static void sendDistance() {
  const uint8_t mpg[] = {count, 0xFF, 0x64, 0x64, 0x64, 0x01, 0xF1};
  sendCrc(0x2C4, mpg, 7, 0xC6);
  const uint8_t mpg2[] = {tick(0xF0), (uint8_t)distance, (uint8_t)(distance >> 8), 0xF2};
  sendCrc(0x2BB, mpg2, 4, 0xDE);
  distance += (uint16_t)(speedKmh * 2.9f);
}

static void sendCheckControl(uint8_t id, bool show) {
  const uint8_t b[] = {0x40, id, 0x00, (uint8_t)(show ? 0x29 : 0x28), 0xFF, 0xFF, 0xFF, 0xFF};
  can1.send(0x5C0, 8, b);
}

static void sendAcc() {
  const uint8_t b[] = {(uint8_t)(0xF0 | accCounter), 0x5C, 0x70, 0x00, 0x00};
  sendCrc(0x33B, b, 5, 0x6B);
  accCounter += 4;
  if (accCounter > 0x0E) accCounter -= 0x0F;
}

static void sendLights() {
  const uint8_t status = (highBeam ? 0x02 : 0) | (mainLights ? 0x04 : 0) | (frontFog ? 0x20 : 0) | (rearFog ? 0x40 : 0);
  const uint8_t lights[] = {status, 0xC0, 0xF7};
  can1.send(0x21A, 3, lights);
  const uint8_t blink = (!turn.leftOut && !turn.rightOut) ? 0x80 : (0x81 | (turn.leftOut ? 0x10 : 0) | (turn.rightOut ? 0x20 : 0));
  const uint8_t blinkers[] = {blink, 0xF0};
  can1.send(0x1F6, 2, blinkers);
}

static void sendSlow() {
  sendLights();
  const uint8_t bl[] = {(uint8_t)map(backlight, 0, 100, 0, 253), 0xFF};
  can1.send(0x202, 2, bl);
  const uint8_t mode[] = {tick(0xF0), 0x00, 0x00, (uint8_t)driveMode, 0x11, 0xC0};
  sendCrc(0x3A7, mode, 6, 0x4A);
  const uint8_t button[] = {0x00, 0xFF};
  can1.send(0x1EE, 2, button);
}

static void sendFast(uint32_t now) {
  sendIgnition((int32_t)(now - ignOnAt) >= 0);
  sendSpeed();
  sendRpm();
  sendBasicInfo();
  sendTransmission();
  sendFuel();
  sendParkBrake();
  sendDistance();
  for (const Lamp &l : lamps) {
    if (l.kind == KIND_ALERT) sendCheckControl(l.ccId, l.on);
  }
  sendAcc();
  counter = counter >= 13 ? 0 : counter + 1;
  count = count >= 253 ? 0 : count + 1;
}

static void stop() {
  for (const Lamp &l : lamps) {
    if (running && l.kind == KIND_CC && l.on) sendCheckControl(l.ccId, false);
  }
  running = false;
  rpm = speedKmh = 0;
  tempC = 100;
  fuelPct = 100;
  backlight = 100;
  driveMode = 2;
  gear = GEAR_P;
  highBeam = frontFog = rearFog = false;
  mainLights = true;
  turn.set(true, true, TURN_OFF);
  turn.update();
  for (Lamp &l : lamps) l.on = false;
}

static void begin() {}

static void update() {
  const bool changed = turn.update();
  if (!running) return;
  if (changed) sendLights();
  const uint32_t now = millis();
  if (now - lastFast >= FAST_MS) {
    lastFast = now;
    sendFast(now);
  }
  if (now - lastSlow >= SLOW_MS) {
    lastSlow = now;
    sendSlow();
  }
  if (now - lastCc >= CC_REFRESH_MS) {
    lastCc = now;
    for (const Lamp &l : lamps) {
      if (l.kind == KIND_CC && l.on) sendCheckControl(l.ccId, true);
    }
  }
  if (now - lastDate >= DATE_MS) {
    lastDate = now;
    const uint8_t date[] = {0x0C, 0x00, 0x00, 0x02, 0x4F, 0xE9, 0x07, 0xF2};
    can1.send(0x39E, 8, date);
  }
  if (now - last5s >= SLOW5S_MS) {
    last5s = now;
    const uint8_t sos[] = {sosCounter, 0x15, 0x0F, 0x00, 0xFF, 0x70, 0xFF, 0xFF};
    can1.send(0x2C3, 8, sos);
    sosCounter = sosCounter >= 14 ? 0 : sosCounter + 1;
    const uint8_t language[] = {0x02, 0, 0, 0, 0, 0, 0, 0};
    can1.send(0x291, 8, language);
  }
}

static const char *note() { return running ? "" : " (ign off, applies at ign on)"; }

static bool lightsArg(const char *name, const char *a, bool &var) {
  bool on;
  if (!onOffArg(a, on)) return true;
  var = on;
  say("F10 %s %s%s", name, on ? "on" : "off", note());
  if (running) sendLights();
  return true;
}

static bool command(int argc, char **argv) {
  const char *cmd = argv[0];
  const char *a = argc > 1 ? argv[1] : "";
  float v;
  bool on;
  if (!strcmp(cmd, "stop")) {
    stop();
    say("F10 stopped, ign off");
  } else if (turnCommand(turn, "F10", argc, argv)) {
  } else if (!strcmp(cmd, "ign")) {
    if (!onOffArg(a, on)) return true;
    if (on && !can1.ok()) {
      rej("CAN1 not initialised, try can1 init");
      return true;
    }
    if (on && !running) ignOnAt = millis() + IGN_CYCLE_MS;
    running = on;
    say(on ? "F10 ignition frames on, screen wakes after about 1.5 s" : "F10 ignition frames off");
  } else if (!strcmp(cmd, "rpm")) {
    if (numArg(a, 0, 7500, v)) {
      rpm = (int)v;
      say("F10 rpm = %d%s", rpm, note());
    }
  } else if (!strcmp(cmd, "speed")) {
    if (numArg(a, 0, 260, v)) {
      speedKmh = (int)v;
      say("F10 speed = %d km/h%s", speedKmh, note());
    }
  } else if (!strcmp(cmd, "fuel")) {
    if (numArg(a, 0, 100, v)) {
      fuelPct = (int)v;
      say("F10 fuel = %d %%%s", fuelPct, note());
    }
  } else if (!strcmp(cmd, "temp")) {
    if (numArg(a, 50, 150, v)) {
      tempC = (int)v;
      say("F10 temp = %d C%s", tempC, note());
    }
  } else if (!strcmp(cmd, "backlight")) {
    if (numArg(a, 0, 100, v)) {
      backlight = (int)v;
      say("F10 backlight = %d %%%s", backlight, note());
    }
  } else if (!strcmp(cmd, "mode")) {
    if (!numArg(a, 1, 7, v)) return true;
    if ((int)v == 3) {
      rej("mode 3 leaves the screen black");
      return true;
    }
    driveMode = (int)v;
    say("F10 drive mode = %d%s", driveMode, note());
  } else if (!strcmp(cmd, "gear")) {
    if (!strcmp(a, "p")) gear = GEAR_P;
    else if (!strcmp(a, "r")) gear = GEAR_R;
    else if (!strcmp(a, "n")) gear = GEAR_N;
    else if (!strcmp(a, "d") || !strcmp(a, "s")) gear = GEAR_D;
    else if (!strcmp(a, "clear")) gear = GEAR_CLEAR;
    else if (strlen(a) == 1 && a[0] >= '1' && a[0] <= '9') gear = a[0] - '0';
    else {
      rej("expected p r n d s clear or 1-9");
      return true;
    }
    say("F10 gear = %s%s", a, note());
  } else if (!strcmp(cmd, "hb")) {
    return lightsArg("high beam", a, highBeam);
  } else if (!strcmp(cmd, "lights")) {
    return lightsArg("main lights", a, mainLights);
  } else if (!strcmp(cmd, "ffog")) {
    return lightsArg("front fog", a, frontFog);
  } else if (!strcmp(cmd, "rfog")) {
    return lightsArg("rear fog", a, rearFog);
  } else if (!strcmp(cmd, "lamp")) {
    if (argc != 3) {
      rej("use: lamp <name> <on|off>, see help");
      return true;
    }
    if (!onOffArg(argv[2], on)) return true;
    for (Lamp &l : lamps) {
      if (!strcmp(a, l.name)) {
        l.on = on;
        if (running && l.kind == KIND_CC) sendCheckControl(l.ccId, on);
        say("F10 %s %s%s", a, on ? "on" : "off", note());
        return true;
      }
    }
    rej("unknown lamp, see help");
  } else if (!strcmp(cmd, "cc")) {
    if (argc != 3) {
      rej("use: cc <0-255> <on|off>");
      return true;
    }
    if (!numArg(a, 0, 255, v) || !onOffArg(argv[2], on)) return true;
    if (!running) {
      rej("ign is off");
      return true;
    }
    sendCheckControl((uint8_t)v, on);
    say("F10 check control %d %s", (int)v, on ? "show" : "clear");
  } else {
    return false;
  }
  return true;
}

static void help() {
  static const HelpRow rows[] = {
    {"ign <on|off>", "start/stop wake-up and heartbeat frames"},
    {"rpm <n>", "0-7500"},
    {"speed <kmh>", "0-260"},
    {"fuel <0-100>", "percent, default 100"},
    {"temp <C>", "50-150, default 100"},
    {"gear <p|r|n|d|s|1-9|clear>", "selector and manual gear"},
    {"left <on|off|blink>", "about 1.5 Hz"},
    {"right <on|off|blink>", "about 1.5 Hz"},
    {"hazard <on|off|blink>", "both, in sync"},
    {"hb <on|off>", "high beam"},
    {"lights <on|off>", "main lights, default on, also night illumination"},
    {"ffog | rfog <on|off>", "front and rear fog lamps"},
    {"backlight <0-100>", "default 100"},
    {"mode <1-7>", "1 traction 2 comfort 4 sport 5 sport+ 6 DSC off 7 eco, not 3"},
    {"lamp <name> <on|off>", "dscoff dscactive dsc35 parkbrake parkfault cc71 cel overheat seatbelt door"},
    {"cc <0-255> <on|off>", "raw Check Control message on 0x5C0"},
    {"stop", "F10 back to its safe state, ign off"},
  };
  printHelp("F10 commands:", rows, sizeof(rows) / sizeof(rows[0]));
}

static void status() {
  say("F10: ign %s, rpm %d, speed %d km/h, temp %d C, fuel %d %%, gear %u, backlight %d %%, left %s, right %s",
      running ? "on" : "off", rpm, speedKmh, tempC, fuelPct, gear, backlight, turnText(turn.left), turnText(turn.right));
}

const Cluster F10 = {"F10", "F10> ", begin, update, stop, command, help, status};

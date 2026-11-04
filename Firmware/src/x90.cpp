#include "board.h"

// TODO cal tables

static constexpr uint32_t SWEEP_MS = 10000, SWEEP_TICK_MS = 50;
static constexpr float SWEEP_RPM = 8000;

static constexpr int32_t X27_STEPS_PER_DEG = 12;
static constexpr int32_t X27_TRAVEL = 3780;
static constexpr int32_t X27_HOME_STEPS = 3900;  // TODO verify
static constexpr float X27_MAX_HZ = 7200;
static constexpr float X27_START_HZ = 100;  // TODO verify
static constexpr float X27_ACCEL = 2000;  // TODO verify
static constexpr float X27_HOME_HZ = 400;  // TODO verify

static SquareWave tach(PIN_X90_TACH, LEDC_X90_TACH);
static Turn turn;
static int cyl = 4;
static float rpm = 0;
static bool sweeping = false;
static uint32_t sweepStart = 0, sweepTick = 0;

static int32_t pos = 0, target = 0;
static int8_t moving = 0;
static float rateHz = 1000, stepHz = 0;
static float degPerKmh = 315.0f / 160;  // TODO verify
static bool homing = false, homed = false, reversed = false;
static uint32_t lastStep = 0, stepPeriodUs = 0;

struct NamedBit { const char *name; uint8_t bit; };
static const NamedBit LAMPS[] = {
  {"ill", X90_ILL}, {"hb", X90_HB}, {"brake", X90_BRAKE}, {"cel", X90_CEL}, {"oil", X90_OIL},
  {"power", X90_POWER}, {"charge", X90_CHARGE}, {"seatbelt", X90_SEATBELT}, {"4wd", X90_4WD},
};

static float tachHz() { return rpm * cyl / 120; }

static float stopDistance() {
  return stepHz > X27_START_HZ ? (stepHz * stepHz - X27_START_HZ * X27_START_HZ) / (2 * X27_ACCEL) : 0;
}

static void x27Halt() {
  if (homing) homed = false;
  homing = false;
  target = pos + moving * (int32_t)(stopDistance() + 1);
  if (!moving) target = pos;
}

static void x27Loop() {
  if (!moving && pos == target) return;
  const uint32_t now = micros();
  if (now - lastStep < stepPeriodUs) return;
  lastStep = now;

  const int32_t rem = target - pos;
  const int8_t want = rem > 0 ? 1 : rem < 0 ? -1 : 0;

  if (!moving || (want != moving && stepHz <= X27_START_HZ)) {
    moving = want;
    stepHz = moving ? X27_START_HZ : 0;
    if (moving) {
      // TODO verify polarity
      digitalWrite(PIN_X27_DIR, (moving > 0) != reversed ? HIGH : LOW);
      stepPeriodUs = 1000000UL / X27_START_HZ;
    } else {
      stepPeriodUs = 0;
      if (homing) {
        homing = false;
        homed = true;
        pos = target = 0;
        say("X90 x27 home done, position 0");
      }
    }
    return;
  }

  digitalWrite(PIN_X27_STEP, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_X27_STEP, LOW);
  pos += moving;

  const float limit = homing ? X27_HOME_HZ : rateHz;
  const bool brake = want != moving || abs(target - pos) <= stopDistance() || stepHz > limit;
  stepHz += (brake ? -X27_ACCEL : X27_ACCEL) / stepHz;
  stepHz = constrain(stepHz, X27_START_HZ, max(limit, X27_START_HZ));
  stepPeriodUs = 1000000.0f / stepHz;
}

static void x27MoveDeg(float deg) {
  target = constrain((int32_t)(deg * X27_STEPS_PER_DEG + 0.5f), 0, X27_TRAVEL);
}

static void clearOutputs() {
  sweeping = false;
  rpm = 0;
  tach.set(0);
  pcaDuty(PCA_X90_TEMP, 0);
  pcaDuty(PCA_X90_FUEL, 0);
  x27Halt();
}

static void stop() {
  clearOutputs();
  pcaDuty(PCA_X90_ODOFF, 0);
  turn.set(true, true, TURN_OFF);
  for (const NamedBit &l : LAMPS) tpicSet(l.bit, false);
  tpicSet(X90_LEFT, false);
  tpicSet(X90_RIGHT, false);
}

static void begin() { tach.begin(); }

static void update() {
  if (turn.update()) {
    tpicSet(X90_LEFT, turn.leftOut);
    tpicSet(X90_RIGHT, turn.rightOut);
  }
  x27Loop();
  if (!sweeping) return;
  const uint32_t now = millis();
  if (now - sweepTick < SWEEP_TICK_MS) return;
  sweepTick = now;
  if (now - sweepStart >= SWEEP_MS) {
    clearOutputs();
    target = 0;
    say("X90 sweep done");
    return;
  }
  const float f = triangle(now - sweepStart, SWEEP_MS);
  rpm = f * SWEEP_RPM;
  tach.set(tachHz());
  pcaDuty(PCA_X90_TEMP, f * 100);
  pcaDuty(PCA_X90_FUEL, f * 100);
  x27MoveDeg(f * X27_TRAVEL / X27_STEPS_PER_DEG);
}

static void gauge(const char *name, uint8_t ch, const char *arg) {
  float pct;
  if (!numArg(arg, 0, 100, pct)) return;
  pcaDuty(ch, pct);
  say("X90 %s = %.1f %% duty", name, pct);
  pcaWarn();
}

static const char *homeNote() { return homed ? "" : " (not homed)"; }

static void x27Command(int argc, char **argv) {
  const char *sub = argc > 1 ? argv[1] : "";
  const char *a = argc > 2 ? argv[2] : "";
  float v;
  if (!strcmp(sub, "home")) {
    homing = true;
    homed = false;
    target = pos - X27_HOME_STEPS;
    say("X90 x27 homing CCW, %u steps at %.0f Hz", (unsigned)X27_HOME_STEPS, X27_HOME_HZ);
  } else if (!strcmp(sub, "dir")) {
    if (!strcmp(a, "normal") || !strcmp(a, "reverse")) {
      reversed = !strcmp(a, "reverse");
      say("X90 x27 dir = %s", a);
    } else {
      rej("expected normal or reverse");
    }
  } else if (!strcmp(sub, "rate")) {
    if (numArg(a, X27_START_HZ, X27_MAX_HZ, v)) {
      rateHz = v;
      say("X90 x27 rate = %.0f Hz (%.0f deg/s)", rateHz, rateHz / X27_STEPS_PER_DEG);
    }
  } else if (homing) {
    rej("homing, wait or use stop");
  } else if (!strcmp(sub, "step")) {
    if (numArg(a, -4000, 4000, v)) {
      target += (int32_t)v;
      say("X90 x27 step %+d, target %d%s", (int)v, (int)target, homeNote());
    }
  } else if (!strcmp(sub, "deg")) {
    if (numArg(a, 0, 315, v)) {
      x27MoveDeg(v);
      say("X90 x27 = %.1f deg (%d steps)%s", v, (int)target, homeNote());
    }
  } else {
    rej("use: x27 home, step <+-n>, deg <0-315>, dir <normal|reverse>, rate <Hz>");
  }
}

static bool command(int argc, char **argv) {
  const char *cmd = argv[0];
  const char *a = argc > 1 ? argv[1] : "";
  float v;
  bool on;
  if (sweeping && strcmp(cmd, "stop")) {
    rej("sweep running, use stop");
    return true;
  }
  if (!strcmp(cmd, "stop")) {
    stop();
    say("X90 stopped");
  } else if (turnCommand(turn, "X90", argc, argv)) {
  } else if (!strcmp(cmd, "x27")) {
    x27Command(argc, argv);
  } else if (!strcmp(cmd, "sweep")) {
    if (homing) {
      rej("homing, wait or use stop");
      return true;
    }
    sweeping = true;
    sweepStart = millis();
    sweepTick = 0;
    say("X90 sweep, %u s, stop to cancel%s", (unsigned)(SWEEP_MS / 1000), homeNote());
  } else if (!strcmp(cmd, "tach")) {
    if (numArg(a, 0, 9000, v)) {
      rpm = v;
      say("X90 tach = %.0f rpm (%s)", rpm, hzText(tachHz(), tach.set(tachHz())));
    }
  } else if (!strcmp(cmd, "cyl")) {
    if (numArg(a, 1, 12, v)) {
      cyl = (int)v;
      say("X90 cyl = %d, tach %s", cyl, hzText(tachHz(), tach.set(tachHz())));
    }
  } else if (!strcmp(cmd, "temp")) {
    gauge("temp", PCA_X90_TEMP, a);
  } else if (!strcmp(cmd, "fuel")) {
    gauge("fuel", PCA_X90_FUEL, a);
  } else if (!strcmp(cmd, "speed")) {
    if (homing) {
      rej("homing, wait or use stop");
    } else if (numArg(a, 0, 160, v)) {
      x27MoveDeg(v * degPerKmh);
      say("X90 speed = %.0f km/h (%.1f deg)%s", v, target / (float)X27_STEPS_PER_DEG, homeNote());
    }
  } else if (!strcmp(cmd, "scale")) {
    if (numArg(a, 0.1f, 10, v)) {
      degPerKmh = v;
      say("X90 scale = %.3f deg per km/h", degPerKmh);
    }
  } else if (!strcmp(cmd, "lamp")) {
    if (argc != 3) {
      rej("use: lamp <name> <on|off>");
      return true;
    }
    if (!strcmp(a, "odoff")) {
      if (onOffArg(argv[2], on)) {
        pcaDuty(PCA_X90_ODOFF, on ? 100 : 0);
        say("X90 odoff lamp %s", on ? "on" : "off");
        pcaWarn();
      }
      return true;
    }
    for (const NamedBit &l : LAMPS) {
      if (!strcmp(a, l.name)) {
        if (onOffArg(argv[2], on)) {
          tpicSet(l.bit, on);
          say("X90 %s lamp %s", l.name, on ? "on" : "off");
        }
        return true;
      }
    }
    rej("unknown lamp: ill hb brake cel oil power charge seatbelt 4wd odoff");
  } else if (!strcmp(cmd, "lamps")) {
    if (onOffArg(a, on)) {
      for (const NamedBit &l : LAMPS) tpicSet(l.bit, on);
      pcaDuty(PCA_X90_ODOFF, on ? 100 : 0);
      turn.set(true, true, on ? TURN_ON : TURN_OFF);
      say("X90 all lamps %s", on ? "on" : "off");
    }
  } else {
    return false;
  }
  return true;
}

static void help() {
  static const HelpRow rows[] = {
    {"tach <rpm>", "0-9000, Hz = rpm x cyl / 120, 0 stops (GPIO33 low)"},
    {"cyl <n>", "1-12, default 4"},
    {"temp <0-100>", "raw duty, more = hotter"},
    {"fuel <0-100>", "raw duty, more = fuller, slow to settle"},
    {"x27 home", "CCW past the end stop at low speed, then position 0"},
    {"x27 step <+-n>", "-4000 to 4000 microsteps, 12 per degree"},
    {"x27 deg <0-315>", "absolute position"},
    {"x27 dir <normal|reverse>", "swap if the coil wiring is flipped"},
    {"x27 rate <Hz>", "100-7200 max step rate, default 1000"},
    {"speed <kmh>", "0-160, through scale, limited to 315 deg"},
    {"scale <deg per km/h>", "0.1-10, default 1.969 (315 deg / 160 km/h)"},
    {"lamp <name> <on|off>", "ill hb brake cel oil power charge seatbelt 4wd odoff"},
    {"left <on|off|blink>", "about 1.5 Hz"},
    {"right <on|off|blink>", "about 1.5 Hz"},
    {"hazard <on|off|blink>", "both, in sync"},
    {"lamps <on|off>", "every X-90 lamp. Illumination may exceed 150 mA"},
    {"sweep", "tach, x27 and gauges 0 -> max -> 0 over 10 s"},
    {"stop", "X-90 back to its safe state, needle stops where it is"},
  };
  printHelp("X90 commands:", rows, sizeof(rows) / sizeof(rows[0]));
}

static void status() {
  say("X90: tach %.0f rpm (%u Hz, %d cyl), temp %.1f, fuel %.1f %%, x27 %.1f deg (target %.1f, %s%s, %.0f Hz max, %s), left %s, right %s%s",
      rpm, (unsigned)tach.hz(), cyl, pcaGetDuty(PCA_X90_TEMP), pcaGetDuty(PCA_X90_FUEL), pos / (float)X27_STEPS_PER_DEG,
      target / (float)X27_STEPS_PER_DEG, homed ? "homed" : "not homed", homing ? ", homing" : "", rateHz,
      reversed ? "reverse" : "normal", turnText(turn.left), turnText(turn.right), sweeping ? ", sweeping" : "");
}

const Cluster X90 = {"X90", "X90> ", begin, update, stop, command, help, status};

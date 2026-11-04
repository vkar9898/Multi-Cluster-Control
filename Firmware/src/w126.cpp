#include "board.h"

// TODO cal tables

static constexpr uint32_t SWEEP_MS = 10000, SWEEP_TICK_MS = 50;
static constexpr float SWEEP_RPM = 6000;
static constexpr float SWEEP_SPEED = 120;  // TODO verify

static SquareWave tach(PIN_W126_TACH, LEDC_W126_TACH), speedo(PIN_W126_SPD, LEDC_W126_SPD);
static Turn turn;
static int cyl = 6;
static float rpm = 0, speedValue = 0, k = 4480;
static bool sweeping = false;
static uint32_t sweepStart = 0, sweepTick = 0;

struct NamedBit { const char *name; uint8_t bit; };
static const NamedBit LAMPS[] = {
  {"battery", W126_BATTERY}, {"brakefluid", W126_BRAKEFLUID}, {"padwear", W126_PADWEAR}, {"coolant", W126_COOLANT},
  {"washer", W126_WASHER}, {"fuelres", W126_FUELRES}, {"abs", W126_ABS}, {"bulbfail", W126_BULBFAIL},
};

static float tachHz() { return rpm * cyl / 120; }
static float speedHz() { return speedValue * k / 3600; }

static void clearOutputs() {
  sweeping = false;
  rpm = speedValue = 0;
  tach.set(0);
  speedo.set(0);
  pcaDuty(PCA_W126_TEMP, 0);
  pcaDuty(PCA_W126_FUEL, 0);
  pcaDuty(PCA_W126_OIL, 0);
}

static void stop() {
  clearOutputs();
  turn.set(true, true, TURN_OFF);
  for (const NamedBit &l : LAMPS) tpicSet(l.bit, false);
  for (uint8_t bit : {W126_OILLEVEL, W126_ILL, W126_HB, W126_LEFT, W126_RIGHT}) tpicSet(bit, false);
}

static void begin() {
  tach.begin();
  speedo.begin();
}

static void update() {
  if (turn.update()) {
    tpicSet(W126_LEFT, turn.leftOut);
    tpicSet(W126_RIGHT, turn.rightOut);
  }
  if (!sweeping) return;
  const uint32_t now = millis();
  if (now - sweepTick < SWEEP_TICK_MS) return;
  sweepTick = now;
  if (now - sweepStart >= SWEEP_MS) {
    clearOutputs();
    say("W126 sweep done");
    return;
  }
  const float f = triangle(now - sweepStart, SWEEP_MS);
  rpm = f * SWEEP_RPM;
  speedValue = f * SWEEP_SPEED;
  tach.set(tachHz());
  speedo.set(speedHz());
  pcaDuty(PCA_W126_TEMP, f * 100);
  pcaDuty(PCA_W126_FUEL, f * 100);
  pcaDuty(PCA_W126_OIL, f * 100);
}

static void gauge(const char *name, uint8_t ch, const char *arg) {
  float pct;
  if (!numArg(arg, 0, 100, pct)) return;
  pcaDuty(ch, pct);
  say("W126 %s = %.1f %% duty", name, pct);
  pcaWarn();
}

static void oilLevel(bool on) { tpicSet(W126_OILLEVEL, !on); }

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
    say("W126 stopped (bit 6 clear, oil level lamp reads lit)");
  } else if (turnCommand(turn, "W126", argc, argv)) {
  } else if (!strcmp(cmd, "sweep")) {
    sweeping = true;
    sweepStart = millis();
    sweepTick = 0;
    say("W126 sweep, %u s, stop to cancel", (unsigned)(SWEEP_MS / 1000));
  } else if (!strcmp(cmd, "lamps")) {
    if (onOffArg(a, on)) {
      for (const NamedBit &l : LAMPS) tpicSet(l.bit, on);
      tpicSet(W126_ILL, on);
      tpicSet(W126_HB, on);
      oilLevel(on);
      turn.set(true, true, on ? TURN_ON : TURN_OFF);
      say("W126 all lamps %s", on ? "on" : "off");
    }
  } else if (!strcmp(cmd, "tach")) {
    if (numArg(a, 0, 9000, v)) {
      rpm = v;
      say("W126 tach = %.0f rpm (%s)", rpm, hzText(tachHz(), tach.set(tachHz())));
    }
  } else if (!strcmp(cmd, "cyl")) {
    if (numArg(a, 1, 12, v)) {
      cyl = (int)v;
      say("W126 cyl = %d, tach %s", cyl, hzText(tachHz(), tach.set(tachHz())));
    }
  } else if (!strcmp(cmd, "speed")) {
    if (numArg(a, 0, 300, v)) {
      speedValue = v;
      say("W126 speed = %.1f (%s)", speedValue, hzText(speedHz(), speedo.set(speedHz())));
    }
  } else if (!strcmp(cmd, "k")) {
    if (numArg(a, 1, 100000, v)) {
      k = v;
      if (speedValue > 0) say("W126 k = %.1f, speedo %s", k, hzText(speedHz(), speedo.set(speedHz())));
      else say("W126 k = %.1f", k);
    }
  } else if (!strcmp(cmd, "hz")) {
    if (numArg(a, 0, 2000, v)) {
      speedValue = 0;
      say("W126 speedo raw = %s", hzText(v, speedo.set(v)));
    }
  } else if (!strcmp(cmd, "temp")) {
    gauge("temp", PCA_W126_TEMP, a);
  } else if (!strcmp(cmd, "fuel")) {
    gauge("fuel", PCA_W126_FUEL, a);
  } else if (!strcmp(cmd, "oil")) {
    gauge("oil", PCA_W126_OIL, a);
  } else if (!strcmp(cmd, "oillevel")) {
    if (onOffArg(a, on)) {
      oilLevel(on);
      say("W126 oil level lamp %s (bit 6 %s, lamp latches for a few s)", on ? "on" : "off", on ? "clear" : "set");
    }
  } else if (!strcmp(cmd, "ill")) {
    if (onOffArg(a, on)) {
      tpicSet(W126_ILL, on);
      say("W126 illumination %s", on ? "on" : "off");
    }
  } else if (!strcmp(cmd, "hb")) {
    if (onOffArg(a, on)) {
      tpicSet(W126_HB, on);
      say("W126 high beam %s", on ? "on" : "off");
    }
  } else if (!strcmp(cmd, "lamp")) {
    if (argc != 3) {
      rej("use: lamp <name> <on|off>");
      return true;
    }
    for (const NamedBit &l : LAMPS) {
      if (!strcmp(argv[1], l.name)) {
        if (onOffArg(argv[2], on)) {
          tpicSet(l.bit, on);
          say("W126 %s lamp %s", l.name, on ? "on" : "off");
        }
        return true;
      }
    }
    rej("unknown lamp: battery brakefluid padwear coolant washer fuelres abs bulbfail");
  } else {
    return false;
  }
  return true;
}

static void help() {
  static const HelpRow rows[] = {
    {"tach <rpm>", "0-9000, Hz = rpm x cyl / 120, 0 stops (GPIO13 low)"},
    {"cyl <n>", "1-12, default 6"},
    {"speed <value>", "0-300, Hz = value x k / 3600, 0 stops"},
    {"k <pulses>", "1-100000 per km or mile, default 4480 (placeholder)"},
    {"hz <n>", "0-2000, raw speedo frequency"},
    {"temp <0-100>", "raw duty, more = hotter"},
    {"fuel <0-100>", "raw duty, more = fuller"},
    {"oil <0-100>", "raw duty, inverted: more = lower pressure"},
    {"lamp <name> <on|off>", "battery brakefluid padwear coolant washer fuelres abs bulbfail"},
    {"oillevel <on|off>", "handles the inversion, lamp latches for a few s"},
    {"ill <on|off>", "illumination"},
    {"hb <on|off>", "high beam"},
    {"left <on|off|blink>", "about 1.5 Hz"},
    {"right <on|off|blink>", "about 1.5 Hz"},
    {"hazard <on|off|blink>", "both, in sync"},
    {"lamps <on|off>", "every W126 lamp"},
    {"sweep", "tach, speedo and gauges 0 -> max -> 0 over 10 s"},
    {"stop", "W126 back to its safe state"},
  };
  printHelp("W126 commands:", rows, sizeof(rows) / sizeof(rows[0]));
}

static void status() {
  say("W126: tach %.0f rpm (%u Hz, %d cyl), speed %.1f (%u Hz, k %.1f), temp %.1f, fuel %.1f, oil %.1f %%, left %s, right %s%s",
      rpm, (unsigned)tach.hz(), cyl, speedValue, (unsigned)speedo.hz(), k, pcaGetDuty(PCA_W126_TEMP), pcaGetDuty(PCA_W126_FUEL),
      pcaGetDuty(PCA_W126_OIL), turnText(turn.left), turnText(turn.right), sweeping ? ", sweeping" : "");
}

const Cluster W126 = {"W126", "W126> ", begin, update, stop, command, help, status};

// GT86 85002CA150, frames from toyota86_cluster_driver/CAN_REFERENCE.md
#include "board.h"

static constexpr uint8_t FUEL_BITS = 8;
static constexpr uint32_t TRIP_SHORT_MS = 200, TRIP_LONG_MS = 2000;
static constexpr float SPEED_SCALE = 0.05796f, SPEED_OFFSET = 1.0f;

struct Frame {
  uint16_t id;
  uint8_t dlc;
  uint16_t periodMs;
  bool bootOn;
  uint8_t seed[8];
  uint8_t d[8];
  bool on;
  uint32_t next;
};

static Frame frames[] = {
  {0x0D0, 8, 20, true, {0x94, 0xFB, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF}},
  {0x0D1, 4, 20, true, {0x00, 0x00, 0x00, 0x00}},
  {0x0D3, 7, 20, true, {0x20, 0x06, 0xC0, 0x0F, 0x00, 0x00, 0xFF}},
  {0x0D4, 8, 20, true, {0}},
  {0x140, 8, 10, true, {0x00, 0x00, 0x68, 0x46, 0x00, 0x00, 0x09, 0x21}},
  {0x141, 8, 10, true, {0x8B, 0x26, 0xFA, 0x27, 0xC9, 0x86, 0xA7, 0x00}},
  {0x144, 8, 20, true, {0xC0, 0x00, 0x4C, 0x17, 0x94, 0xA4, 0xA0, 0x00}},
  {0x152, 8, 20, true, {0}},
  {0x360, 8, 50, true, {0xD7, 0x00, 0x6F, 0x50, 0x32, 0x00, 0x01, 0x00}},
  {0x361, 8, 50, true, {0x00, 0x29, 0x00, 0xD3, 0x00, 0xD0, 0x73, 0x72}},
  {0x370, 8, 50, true, {0x67, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00}},
  {0x372, 8, 100, true, {0}},
  {0x368, 8, 50, false, {0}},
  {0x374, 8, 1000, false, {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

enum CanLamp : uint8_t { L_CE, L_TC, L_TRC, L_SL, L_VS, L_AB, L_ABS, L_C1, L_C2, L_CY, L_HB, L_BK, L_HI, L_FF, L_RF, L_SP, L_AT, L_SN, L_COUNT };
static const char *const LAMP_NAMES[L_COUNT] = {"ce", "tc", "trc", "sl", "vs", "ab", "abs", "c1", "c2", "cy", "hb", "bk", "hi", "ff", "rf", "sp", "at", "sn"};
static bool lamp[L_COUNT];

static bool canOn = false;
static uint16_t rpm = 800, speedKmh = 0, speedRaw = 0;
static int coolantC = 40;
static const int oilC = 71;
static uint8_t d3Rate = 0, ctr140 = 0, ctr361 = 0, ctr372 = 0, ctr370 = 0, ctr374 = 0, ctrD3 = 0, ctrD3Motion = 0;

static Turn turn;
static float fuelPct = 0;
static uint32_t tripUntil = 0;
static bool tripHeld = false;

struct NamedBit { const char *name; uint8_t bit; };
static const NamedBit WIRED_LAMPS[] = {
  {"oil", GT86_OIL}, {"charge", GT86_CHARGE}, {"brake", GT86_BRAKE}, {"security", GT86_SECURITY}, {"seatbelt", GT86_SEATBELT},
};

static void setSpeed(uint16_t kmh) {
  speedKmh = kmh;
  const float eff = max(0.0f, kmh - SPEED_OFFSET);
  speedRaw = (uint16_t)(eff / SPEED_SCALE + 0.5f);
  d3Rate = (kmh + 5) / 10;
}

static uint8_t gearFromSpeed(uint16_t kmh) {
  return kmh < 30 ? 1 : kmh < 60 ? 2 : kmh < 100 ? 3 : kmh < 150 ? 4 : 5;
}

static int16_t triWave(uint32_t t, uint32_t period, int16_t amp) {
  const uint32_t p = t % period;
  const long v = p < period / 2 ? (long)p : (long)(period - p);
  return (int16_t)(v * 4L * amp / (long)period - amp);
}

static void updateFrame(Frame &f) {
  uint8_t *d = f.d;
  switch (f.id) {
    case 0x140: {
      d[0] = 0xFF;
      d[1] = (d[1] & 0xF0) | (ctr140 & 0x0F);
      ctr140 = (ctr140 + 1) & 0x0F;
      const uint16_t w = (((d[3] << 8) | d[2]) & 0x8000) | (rpm & 0x3FFF);
      d[2] = w & 0xFF;
      d[3] = w >> 8;
      d[7] = rpm <= 1200 ? 0x21 : 0x00;
      break;
    }
    case 0x141: {
      const uint16_t raw = rpm + 32768u;
      d[4] = raw & 0xFF;
      d[5] = raw >> 8;
      d[6] = (d[6] & 0xF0) | gearFromSpeed(speedKmh);
      break;
    }
    case 0x360:
      d[2] = (uint8_t)(oilC + 40);
      d[3] = (uint8_t)(coolantC + 40);
      d[5] = (d[5] & ~0x30) | (lamp[L_C1] ? 0x10 : 0) | (lamp[L_C2] ? 0x20 : 0);
      break;
    case 0x361:
      d[1] = lamp[L_CE] ? 0x10 : 0x20;
      d[4] = (d[4] & 0x70) | (ctr361 & 0x0F) | (lamp[L_CY] ? 0x80 : 0);
      ctr361 = (ctr361 + 1) & 0x0F;
      break;
    case 0x372:
      d[2] = lamp[L_AB] ? 0x01 : 0x00;
      d[1] = (d[1] & 0x0F) | (ctr372 & 0xF0);
      ctr372 += 0x10;
      break;
    case 0x152:
      d[6] = (d[6] & ~0x18) | (lamp[L_HB] ? 0x08 : 0) | (lamp[L_BK] ? 0x10 : 0);
      d[7] = (d[7] & ~0x10) | (lamp[L_HI] ? 0x10 : 0);
      break;
    case 0x370:
      d[5] = (d[5] & 0xF0) | (ctr370 & 0x0F);
      ctr370 = (ctr370 + 1) & 0x0F;
      break;
    case 0x368:
      d[1] = (lamp[L_SP] ? 0x10 : 0) | (lamp[L_AT] ? 0x40 : 0);
      d[2] = (d[2] & ~0x04) | (lamp[L_SN] ? 0x04 : 0);
      break;
    case 0x374:
      d[1] = (d[1] & 0x30) | (ctr374 & 0x0F) | (lamp[L_FF] ? 0x40 : 0) | (lamp[L_RF] ? 0x80 : 0);
      ctr374 = (ctr374 + 1) & 0x0F;
      break;
    case 0x0D1:
      d[0] = speedRaw & 0xFF;
      d[1] = speedRaw >> 8;
      d[2] = (uint8_t)(0x29 + triWave(millis(), 2000, 2));
      break;
    case 0x0D3: {
      d[0] = (lamp[L_VS] ? 0x08 : 0) | (lamp[L_TRC] ? 0x20 : 0);
      d[1] = 0x06 | (lamp[L_VS] ? 0x08 : 0) | (lamp[L_TC] ? 0x20 : 0);
      const uint8_t warnings = (lamp[L_SL] ? 0x40 : 0) | (lamp[L_ABS] ? 0x20 : 0);
      d[3] = warnings ? warnings : f.seed[3];
      d[4] = ctrD3Motion;
      if (speedKmh > 0) ctrD3Motion += d3Rate;
      d[6] = ctrD3++;
      break;
    }
    case 0x0D4:
      d[0] = d[2] = d[4] = d[6] = speedRaw & 0xFF;
      d[1] = d[3] = d[5] = d[7] = speedRaw >> 8;
      break;
    default:
      break;
  }
}

static void resetCanState() {
  canOn = false;
  rpm = 800;
  coolantC = 40;
  setSpeed(0);
  memset(lamp, 0, sizeof(lamp));
  for (Frame &f : frames) {
    memcpy(f.d, f.seed, 8);
    f.on = f.bootOn;
  }
}

static void setFuel(float pct) {
  fuelPct = pct;
  ledcWrite(LEDC_GT86_FUEL, (uint32_t)(pct * 2.55f + 0.5f));
}

static void stop() {
  resetCanState();
  setFuel(0);
  turn.set(true, true, TURN_OFF);
  tripUntil = 0;
  tripHeld = false;
  tpicSet(GT86_TRIP, false);
  tpicSet(GT86_LEFT, false);
  tpicSet(GT86_RIGHT, false);
  for (const NamedBit &l : WIRED_LAMPS) tpicSet(l.bit, false);
}

static void begin() {
  digitalWrite(PIN_GT86_FUEL, LOW);
  pinMode(PIN_GT86_FUEL, OUTPUT);
  ledcSetup(LEDC_GT86_FUEL, GT86_FUEL_HZ, FUEL_BITS);
  ledcAttachPin(PIN_GT86_FUEL, LEDC_GT86_FUEL);
  ledcWrite(LEDC_GT86_FUEL, 0);
  resetCanState();
}

static void update() {
  if (turn.update()) {
    tpicSet(GT86_LEFT, turn.leftOut);
    tpicSet(GT86_RIGHT, turn.rightOut);
  }
  const uint32_t now = millis();
  if (tripHeld && (int32_t)(now - tripUntil) >= 0) {
    tripHeld = false;
    tpicSet(GT86_TRIP, false);
    say("GT86 TRIP released");
  }
  if (!canOn) return;
  for (Frame &f : frames) {
    if (!f.on || (int32_t)(now - f.next) < 0) continue;
    f.next += f.periodMs;
    if ((int32_t)(now - f.next) > 0) f.next = now + f.periodMs;
    updateFrame(f);
    can2.send(f.id, f.dlc, f.d);
  }
}

static Frame &frameById(uint16_t id) {
  for (Frame &f : frames) {
    if (f.id == id) return f;
  }
  return frames[0];
}

static void setCanLamp(uint8_t i, bool on) {
  lamp[i] = on;
  if (i == L_SP || i == L_AT || i == L_SN) frameById(0x368).on = true;
  if (i == L_FF || i == L_RF) frameById(0x374).on = true;
}

static const char *canNote() { return canOn ? "" : " (CAN off, applies at can on)"; }

static bool command(int argc, char **argv) {
  const char *cmd = argv[0];
  const char *a = argc > 1 ? argv[1] : "";
  float v;
  bool on;
  if (!strcmp(cmd, "stop")) {
    stop();
    say("GT86 stopped, CAN off");
  } else if (turnCommand(turn, "GT86", argc, argv)) {
  } else if (!strcmp(cmd, "fuel")) {
    if (numArg(a, 0, 100, v)) {
      setFuel(v);
      say("GT86 fuel = %.1f %% duty at %u Hz", v, (unsigned)GT86_FUEL_HZ);
    }
  } else if (!strcmp(cmd, "trip")) {
    const bool longPress = !strcmp(a, "long");
    if (*a && !longPress) {
      rej("use: trip or trip long");
      return true;
    }
    tripUntil = millis() + (longPress ? TRIP_LONG_MS : TRIP_SHORT_MS);
    tripHeld = true;
    tpicSet(GT86_TRIP, true);
    say("GT86 TRIP pressed for %u ms", (unsigned)(longPress ? TRIP_LONG_MS : TRIP_SHORT_MS));
  } else if (!strcmp(cmd, "lamp")) {
    if (argc != 3) {
      rej("use: lamp <oil|charge|brake|security|seatbelt> <on|off>");
      return true;
    }
    for (const NamedBit &l : WIRED_LAMPS) {
      if (!strcmp(a, l.name)) {
        if (onOffArg(argv[2], on)) {
          tpicSet(l.bit, on);
          say("GT86 %s lamp %s", l.name, on ? "on" : "off");
        }
        return true;
      }
    }
    rej("unknown lamp: oil charge brake security seatbelt");
  } else if (!strcmp(cmd, "can")) {
    if (!onOffArg(a, on)) return true;
    if (on && !can2.ok()) {
      rej("CAN2 not initialised, try can2 init");
      return true;
    }
    if (on && !canOn) {
      const uint32_t now = millis();
      for (Frame &f : frames) f.next = now;
    }
    canOn = on;
    say("GT86 CAN frames %s", on ? "on" : "off");
  } else if (!strcmp(cmd, "rpm")) {
    if (numArg(a, 0, 8960, v)) {
      rpm = (uint16_t)v;
      say("GT86 rpm = %u%s", rpm, canNote());
    }
  } else if (!strcmp(cmd, "speed")) {
    if (numArg(a, 0, 320, v)) {
      setSpeed((uint16_t)v);
      say("GT86 speed = %u km/h (raw %u, odo rate %u)%s", speedKmh, speedRaw, d3Rate, canNote());
    }
  } else if (!strcmp(cmd, "temp")) {
    if (numArg(a, -40, 215, v)) {
      coolantC = (int)v;
      say("GT86 coolant = %d C%s", coolantC, canNote());
    }
  } else if (!strcmp(cmd, "canlamp")) {
    if (argc != 3) {
      rej("use: canlamp <name> <on|off>, see help");
      return true;
    }
    if (!onOffArg(argv[2], on)) return true;
    if (!strcmp(a, "cr")) {
      setCanLamp(L_C1, on);
      setCanLamp(L_C2, on);
      say("GT86 cr %s%s", on ? "on" : "off", canNote());
      return true;
    }
    for (uint8_t i = 0; i < L_COUNT; i++) {
      if (!strcmp(a, LAMP_NAMES[i])) {
        setCanLamp(i, on);
        say("GT86 %s %s%s", a, on ? "on" : "off", canNote());
        return true;
      }
    }
    rej("unknown CAN lamp, see help");
  } else {
    return false;
  }
  return true;
}

static void help() {
  static const HelpRow rows[] = {
    {"fuel <0-100>", "raw duty on GPIO14 at 31 kHz, more = fuller"},
    {"left <on|off|blink>", "pulsed +12 V, about 1.5 Hz"},
    {"right <on|off|blink>", "pulsed +12 V, about 1.5 Hz"},
    {"hazard <on|off|blink>", "both, in sync"},
    {"trip", "press TRIP for 200 ms"},
    {"trip long", "hold TRIP for 2 s"},
    {"lamp <name> <on|off>", "hard-wired: oil charge brake security seatbelt"},
    {"can <on|off>", "periodic CAN frames that keep the cluster awake"},
    {"rpm <n>", "0-8960, default 800"},
    {"speed <kmh>", "0-320, also drives the odometer"},
    {"temp <C>", "-40 to 215, default 40"},
    {"canlamp <name> <on|off>", "ce tc trc sl vs ab abs cr c1 c2 cy hb bk hi ff rf sp at sn"},
    {"stop", "GT86 back to its safe state, CAN off"},
  };
  printHelp("GT86 commands:", rows, sizeof(rows) / sizeof(rows[0]));
}

static void status() {
  say("GT86: CAN %s, rpm %u, speed %u km/h, coolant %d C, fuel %.1f %%, left %s, right %s",
      canOn ? "on" : "off", rpm, speedKmh, coolantC, fuelPct, turnText(turn.left), turnText(turn.right));
}

const Cluster GT86 = {"GT86", "GT86> ", begin, update, stop, command, help, status};

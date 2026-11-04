#include <stdarg.h>
#include <Wire.h>
#include "board.h"

static constexpr uint8_t MAX_ARGS = 14;

static char line[120];
static uint8_t lineLen = 0;
static bool overflow = false;
static const Cluster *active = nullptr;

void say(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
}

void rej(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print("! ");
  Serial.println(buf);
}

bool onOffArg(const char *s, bool &out) {
  if (!strcmp(s, "on")) out = true;
  else if (!strcmp(s, "off")) out = false;
  else {
    rej("expected on or off");
    return false;
  }
  return true;
}

bool numArg(const char *s, float lo, float hi, float &out) {
  char *end;
  const float v = strtof(s, &end);
  if (end == s || (*end && strcmp(end, "%"))) {
    rej("'%s' is not a number", s);
    return false;
  }
  if (v < lo || v > hi) {
    rej("out of range, expected %g to %g", lo, hi);
    return false;
  }
  out = v;
  return true;
}

void printHelp(const char *title, const HelpRow *rows, size_t n) {
  say("%s", title);
  for (size_t i = 0; i < n; i++) say("  %-34s %s", rows[i].usage, rows[i].what);
}

static const HelpRow GLOBAL_HELP[] = {
  {"help", "this list"},
  {"status", "state of everything"},
  {"off", "everything back to the boot state"},
  {"reboot", "safe outputs, then restart"},
  {"w126 | x90 | gt86 | f10", "select a cluster"},
  {"back", "no cluster selected"},
  {"tpic <0-31> <on|off>", "set one raw TPIC bit"},
  {"tpic all <on|off>", "set all 32 bits"},
  {"tpic show", "print all bits with names"},
  {"walk [ms] | walk stop", "one bit at a time, default 2000, 50-60000 ms"},
  {"oe <on|off>", "TPIC output enable"},
  {"pca <0-15> <0-100%>", "raw PCA9685 duty"},
  {"pwmfreq <Hz>", "PCA9685 frequency, 24-1526, default 1000"},
  {"light", "light sensor raw ADC and %"},
  {"i2cscan", "scan I2C, starts the PCA9685 if newly found"},
  {"can1|can2 status", "controller state and counters"},
  {"can1|can2 sniff <on|off>", "print received frames"},
  {"can1|can2 send <id> <b0 b1 ...>", "one raw frame, all hex"},
  {"can1|can2 init", "retry a failed MCP2515"},
};

static void cmdHelp() {
  if (active) {
    active->help();
    say("Global commands still work; 'back' then 'help' lists them.");
  } else {
    printHelp("Global commands:", GLOBAL_HELP, sizeof(GLOBAL_HELP) / sizeof(GLOBAL_HELP[0]));
  }
}

static void cmdStatus() {
  say("TPIC OE %s%s, PCA9685 %s at %u Hz", tpicOeOn() ? "on" : "off", walkActive() ? ", walk running" : "",
      pcaOk() ? "ok" : "not found", pcaGetFreq());
  for (uint8_t ch = 0; ch < 16; ch++) {
    if (pcaGetDuty(ch) > 0) say("  pca %u = %.1f %%", ch, pcaGetDuty(ch));
  }
  for (const Cluster *c : CLUSTERS) c->status();
  can1.status();
  can2.status();
}

static void allOff() {
  for (const Cluster *c : CLUSTERS) c->stop();
  tpicSafe();
  pcaFreq(PCA_DEFAULT_HZ);
  pcaAllOff();
  can1.sniffOff();
  can2.sniffOff();
}

static void cmdTpic(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "show")) {
    tpicShow();
    return;
  }
  if (argc != 3) {
    rej("use: tpic <0-31> <on|off>, tpic all <on|off>, tpic show");
    return;
  }
  bool on;
  if (!onOffArg(argv[2], on)) return;
  if (!strcmp(argv[1], "all")) {
    tpicSetAll(on);
    say("all TPIC bits %s", on ? "ON" : "off");
    return;
  }
  float bit;
  if (!numArg(argv[1], 0, 31, bit)) return;
  tpicSet((uint8_t)bit, on);
  say("bit %u = U%u D%u = %s: %s", (uint8_t)bit, (uint8_t)bit / 8 + 1, (uint8_t)bit % 8, tpicName((uint8_t)bit), on ? "ON" : "off");
}

static void cmdWalk(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "stop")) {
    walkStop();
    say("walk stopped, outputs restored");
    return;
  }
  float ms = 2000;
  if (argc > 2 || (argc == 2 && !numArg(argv[1], 50, 60000, ms))) {
    if (argc > 2) rej("use: walk [ms] or walk stop");
    return;
  }
  walkStart((uint32_t)ms);
}

static void cmdPca(int argc, char **argv) {
  float ch, pct;
  if (argc != 3) {
    rej("use: pca <0-15> <0-100%%>");
    return;
  }
  if (!numArg(argv[1], 0, 15, ch) || !numArg(argv[2], 0, 100, pct)) return;
  pcaDuty((uint8_t)ch, pct);
  say("pca %u = %.1f %%", (uint8_t)ch, pct);
  pcaWarn();
}

static void cmdI2cScan() {
  uint8_t found = 0;
  bool pcaSeen = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    const uint8_t result = Wire.endTransmission();
    if (result == 0) {
      say("found 0x%02X%s", addr, addr == PCA_ADDR ? " (PCA9685)" : "");
      found++;
      pcaSeen |= addr == PCA_ADDR;
    } else if (result >= 4) {
      say("I2C bus error %u: SDA or SCL held low, scan stopped", result);
      break;
    }
  }
  say("%u device(s)", found);
  if (!pcaSeen) {
    say("hint: no PCA9685. Everything except the ESP32 needs barrel jack 2, so USB alone shows nothing.");
  } else if (!pcaOk()) {
    pcaBegin();
    say("PCA9685 started, all channels 0 %%");
  }
}

static void dispatch(int argc, char **argv) {
  const char *cmd = argv[0];
  if (!strcmp(cmd, "help")) cmdHelp();
  else if (!strcmp(cmd, "status")) cmdStatus();
  else if (!strcmp(cmd, "off")) { allOff(); say("everything off, boot state"); }
  else if (!strcmp(cmd, "reboot")) { allOff(); say("rebooting"); Serial.flush(); ESP.restart(); }
  else if (!strcmp(cmd, "back")) { active = nullptr; say("no cluster selected"); }
  else if (!strcmp(cmd, "tpic")) cmdTpic(argc, argv);
  else if (!strcmp(cmd, "walk")) cmdWalk(argc, argv);
  else if (!strcmp(cmd, "oe")) {
    bool on;
    if (argc != 2) rej("use: oe <on|off>");
    else if (onOffArg(argv[1], on)) { tpicOe(on); say("OE %s", on ? "on (GPIO32 low)" : "off (GPIO32 high)"); }
  }
  else if (!strcmp(cmd, "pca")) cmdPca(argc, argv);
  else if (!strcmp(cmd, "pwmfreq")) {
    float hz;
    if (argc != 2) rej("use: pwmfreq <Hz>");
    else if (numArg(argv[1], PCA_MIN_HZ, PCA_MAX_HZ, hz)) { pcaFreq((uint16_t)hz); say("PCA9685 = %u Hz", (uint16_t)hz); pcaWarn(); }
  }
  else if (!strcmp(cmd, "light")) {
    const int raw = analogRead(PIN_LIGHT);
    say("light raw %d, about %.0f %%", raw, raw * 100.0f / 4095);
  }
  else if (!strcmp(cmd, "i2cscan")) cmdI2cScan();
  else if (!strcmp(cmd, "can1")) can1.command(argc, argv);
  else if (!strcmp(cmd, "can2")) can2.command(argc, argv);
  else {
    for (const Cluster *c : CLUSTERS) {
      if (!strcasecmp(cmd, c->name)) {
        active = c;
        say("%s selected, 'help' lists its commands", c->name);
        return;
      }
    }
    if (!active) rej("unknown command, try help");
    else if (!active->command(argc, argv)) rej("unknown %s command, try help", active->name);
  }
}

static void prompt() {
  Serial.print(active ? active->prompt : "> ");
}

void consoleBegin() {
  say("type help");
  prompt();
}

void consoleLoop() {
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (overflow) {
        rej("line too long");
      } else if (lineLen) {
        line[lineLen] = 0;
        char *argv[MAX_ARGS];
        int argc = 0;
        for (char *tok = strtok(line, " \t"); tok && argc < MAX_ARGS; tok = strtok(nullptr, " \t")) argv[argc++] = tok;
        if (argc) dispatch(argc, argv);
      } else if (ch == '\r') {
        continue;
      }
      lineLen = 0;
      overflow = false;
      prompt();
    } else if (ch == '\b' || ch == 127) {
      if (lineLen) lineLen--;
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = tolower(ch);
    } else {
      overflow = true;
    }
  }
}

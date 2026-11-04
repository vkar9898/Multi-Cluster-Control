#include "board.h"

static constexpr uint32_t REFRESH_MS = 100;

static uint32_t state = 0;
static bool dirty = false;
static bool oeOn = false;
static uint32_t lastPush = 0;

static bool walking = false;
static uint8_t walkBit = 0;
static uint32_t walkMs = 0, walkStep = 0;

static void push() {
  const uint32_t out = walking ? (1UL << walkBit) : state;
  for (int8_t i = 31; i >= 0; i--) {
    digitalWrite(PIN_TPIC_SER, (out >> i) & 1);
    digitalWrite(PIN_TPIC_CLK, HIGH);
    digitalWrite(PIN_TPIC_CLK, LOW);
  }
  digitalWrite(PIN_TPIC_LAT, HIGH);
  digitalWrite(PIN_TPIC_LAT, LOW);
  dirty = false;
  lastPush = millis();
}

void tpicBegin() {
  digitalWrite(PIN_TPIC_OE, HIGH);
  pinMode(PIN_TPIC_OE, OUTPUT);
  for (uint8_t pin : {PIN_TPIC_SER, PIN_TPIC_CLK, PIN_TPIC_LAT}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  tpicSafe();
}

void tpicSafe() {
  walking = false;
  state = 0;
  push();
  tpicOe(true);
}

void tpicLoop() {
  const uint32_t now = millis();
  if (walking && now - walkStep >= walkMs) {
    if (walkBit == 31) {
      walking = false;
      say("walk done, outputs restored");
    } else {
      walkBit++;
      walkStep = now;
      tpicDescribe(walkBit);
    }
    dirty = true;
  }
  if (dirty || now - lastPush >= REFRESH_MS) push();
}

void tpicSet(uint8_t bit, bool on) {
  if (on) state |= 1UL << bit;
  else state &= ~(1UL << bit);
  dirty = true;
}

void tpicSetAll(bool on) {
  state = on ? 0xFFFFFFFFUL : 0;
  dirty = true;
}

bool tpicGet(uint8_t bit) { return (state >> bit) & 1; }

void tpicOe(bool enabled) {
  oeOn = enabled;
  digitalWrite(PIN_TPIC_OE, enabled ? LOW : HIGH);
}

bool tpicOeOn() { return oeOn; }

const char *tpicName(uint8_t bit) {
  for (const TpicName &n : TPIC_NAMES) {
    if (n.bit == bit) return n.name;
  }
  return "?";
}

void tpicDescribe(uint8_t bit) {
  say("bit %u = U%u D%u = %s", bit, bit / 8 + 1, bit % 8, tpicName(bit));
}

void tpicShow() {
  for (uint8_t bit = 0; bit < 32; bit++) {
    say("%2u  U%u D%u  %-3s  %s", bit, bit / 8 + 1, bit % 8, tpicGet(bit) ? "ON" : "off", tpicName(bit));
  }
  say("OE %s%s", oeOn ? "on" : "off", walking ? ", walk running (outputs overridden)" : "");
}

void walkStart(uint32_t ms) {
  walking = true;
  walkBit = 0;
  walkMs = ms;
  walkStep = millis();
  dirty = true;
  say("walk: %u ms per bit, 'walk stop' to cancel", (unsigned)ms);
  if (!oeOn) say("warning: OE is off, nothing will light");
  tpicDescribe(0);
}

void walkStop() {
  walking = false;
  dirty = true;
}

bool walkActive() { return walking; }

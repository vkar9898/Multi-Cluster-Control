#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "board.h"

static constexpr uint8_t TONE_BITS = 10;

static Adafruit_PWMServoDriver pca(PCA_ADDR, Wire);
static bool pcaFound = false;
static uint16_t pcaHz = PCA_DEFAULT_HZ;
static float duty[16] = {};

void pcaBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  Wire.beginTransmission(PCA_ADDR);
  pcaFound = Wire.endTransmission() == 0;
  if (!pcaFound) return;
  pca.begin();
  pcaFreq(PCA_DEFAULT_HZ);
  pcaAllOff();
}

bool pcaOk() { return pcaFound; }

void pcaDuty(uint8_t ch, float pct) {
  duty[ch] = pct;
  if (!pcaFound) return;
  if (pct <= 0) pca.setPWM(ch, 0, 4096);
  else if (pct >= 100) pca.setPWM(ch, 4096, 0);
  else pca.setPWM(ch, 0, (uint16_t)(pct * 40.95f + 0.5f));
}

float pcaGetDuty(uint8_t ch) { return duty[ch]; }

void pcaFreq(uint16_t hz) {
  pcaHz = hz;
  if (pcaFound) pca.setPWMFreq(hz);
}

uint16_t pcaGetFreq() { return pcaHz; }

void pcaAllOff() {
  for (uint8_t ch = 0; ch < 16; ch++) pcaDuty(ch, 0);
}

void pcaWarn() {
  if (!pcaFound) say("warning: PCA9685 not found at 0x%02X. It needs barrel jack 2; run i2cscan once powered.", PCA_ADDR);
}

void SquareWave::begin() {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
  ledcSetup(channel, 100, TONE_BITS);
  current = 0;
}

uint32_t SquareWave::set(float hz) {
  if (hz <= 0) {
    if (current) {
      ledcDetachPin(pin);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
    current = 0;
    return 0;
  }
  uint32_t target = (uint32_t)(hz + 0.5f);
  if (target < 1) target = 1;
  if (target == current) return current;
  if (!ledcChangeFrequency(channel, target, TONE_BITS)) return current;
  if (!current) {
    ledcAttachPin(pin, channel);
    ledcWrite(channel, 1 << (TONE_BITS - 1));
  }
  current = target;
  return current;
}

const char *hzText(float wanted, uint32_t actual) {
  static char text[40];
  if (fabsf(wanted - actual) < 0.05f) snprintf(text, sizeof(text), "%.1f Hz", wanted);
  else snprintf(text, sizeof(text), "%.1f Hz, output %u Hz", wanted, (unsigned)actual);
  return text;
}

void Turn::set(bool setLeft, bool setRight, TurnMode mode) {
  const bool wasBlinking = left == TURN_BLINK || right == TURN_BLINK;
  if (setLeft) left = mode;
  if (setRight) right = mode;
  if (mode == TURN_BLINK && !wasBlinking) start = millis();
}

bool Turn::update() {
  const bool phase = ((millis() - start) / TURN_HALF_MS) % 2 == 0;
  const bool l = left == TURN_ON || (left == TURN_BLINK && phase);
  const bool r = right == TURN_ON || (right == TURN_BLINK && phase);
  const bool changed = l != leftOut || r != rightOut;
  leftOut = l;
  rightOut = r;
  return changed;
}

const char *turnText(TurnMode mode) {
  return mode == TURN_BLINK ? "blink" : mode == TURN_ON ? "on" : "off";
}

bool turnCommand(Turn &turn, const char *cluster, int argc, char **argv) {
  const bool left = !strcmp(argv[0], "left"), right = !strcmp(argv[0], "right"), hazard = !strcmp(argv[0], "hazard");
  if (!left && !right && !hazard) return false;
  TurnMode mode;
  if (argc == 2 && !strcmp(argv[1], "on")) mode = TURN_ON;
  else if (argc == 2 && !strcmp(argv[1], "off")) mode = TURN_OFF;
  else if (argc == 2 && !strcmp(argv[1], "blink")) mode = TURN_BLINK;
  else {
    rej("expected on, off or blink");
    return true;
  }
  turn.set(left || hazard, right || hazard, mode);
  say("%s %s = %s", cluster, argv[0], turnText(mode));
  return true;
}

float triangle(uint32_t t, uint32_t periodMs) {
  const uint32_t half = periodMs / 2;
  return t < half ? (float)t / half : (float)(periodMs - t) / half;
}

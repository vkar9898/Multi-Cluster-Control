#pragma once
#include <Arduino.h>
#include <ACAN2515.h>
#include "hw_map.h"

#define FW_VERSION "0.1"

#if ESP_ARDUINO_VERSION_MAJOR != 2
#error "needs Arduino-ESP32 core 2.x (ledcSetup API)"
#endif

void tpicBegin();
void tpicSafe();
void tpicLoop();
void tpicSet(uint8_t bit, bool on);
void tpicSetAll(bool on);
bool tpicGet(uint8_t bit);
void tpicOe(bool enabled);
bool tpicOeOn();
const char *tpicName(uint8_t bit);
void tpicShow();
void tpicDescribe(uint8_t bit);
void walkStart(uint32_t ms);
void walkStop();
bool walkActive();

void pcaBegin();
bool pcaOk();
void pcaDuty(uint8_t ch, float pct);
float pcaGetDuty(uint8_t ch);
void pcaFreq(uint16_t hz);
uint16_t pcaGetFreq();
void pcaAllOff();
void pcaWarn();

class SquareWave {
 public:
  SquareWave(uint8_t pin, uint8_t channel) : pin(pin), channel(channel) {}
  void begin();
  uint32_t set(float hz);
  uint32_t hz() const { return current; }
 private:
  uint8_t pin, channel;
  uint32_t current = 0;
};
const char *hzText(float wanted, uint32_t actual);

enum TurnMode : uint8_t { TURN_OFF, TURN_ON, TURN_BLINK };
struct Turn {
  TurnMode left = TURN_OFF, right = TURN_OFF;
  bool leftOut = false, rightOut = false;
  void set(bool setLeft, bool setRight, TurnMode mode);
  bool update();
 private:
  uint32_t start = 0;
};
const char *turnText(TurnMode mode);
bool turnCommand(Turn &turn, const char *cluster, int argc, char **argv);

float triangle(uint32_t t, uint32_t periodMs);

class CanBus {
 public:
  CanBus(const char *name, const char *owner, uint8_t cs, uint8_t intPin, void (*isr)());
  bool begin();
  void loop();
  bool send(uint32_t id, uint8_t len, const uint8_t *data);
  bool ok() const { return ready; }
  void command(int argc, char **argv);
  void status();
  void sniffOff() { sniff = false; }
  ACAN2515 mcp;
 private:
  const char *name, *owner;
  void (*isr)();
  bool ready = false, sniff = false, fullWarned = false;
  uint16_t lastError = 0;
  uint32_t txCount = 0, txFail = 0, rxCount = 0, dropped = 0;
};
extern CanBus can1, can2;
void canBegin();

void consoleBegin();
void consoleLoop();
void say(const char *fmt, ...);
void rej(const char *fmt, ...);
bool onOffArg(const char *s, bool &out);
bool numArg(const char *s, float lo, float hi, float &out);
struct HelpRow { const char *usage, *what; };
void printHelp(const char *title, const HelpRow *rows, size_t n);

struct Cluster {
  const char *name, *prompt;
  void (*begin)();
  void (*loop)();
  void (*stop)();
  bool (*command)(int argc, char **argv);
  void (*help)();
  void (*status)();
};
extern const Cluster W126, X90, GT86, F10;
extern const Cluster *const CLUSTERS[4];

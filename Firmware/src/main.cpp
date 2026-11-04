#include "board.h"

const Cluster *const CLUSTERS[4] = {&W126, &X90, &GT86, &F10};

void setup() {
  tpicBegin();
  for (uint8_t pin : {PIN_X27_STEP, PIN_X27_DIR}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }

  for (const Cluster *c : CLUSTERS) c->begin();

  Serial.setTxBufferSize(8192);
  Serial.begin(115200);
  say("\nMulti-Cluster-Control bench firmware v%s (Arduino-ESP32 core %d.%d.%d)", FW_VERSION,
      ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);

  pcaBegin();
  pcaWarn();
  canBegin();
  consoleBegin();
}

void loop() {
  tpicLoop();
  can1.loop();
  can2.loop();
  for (const Cluster *c : CLUSTERS) c->loop();
  consoleLoop();
}

#include <SPI.h>
#include "board.h"

CanBus can1("CAN1", "F10", PIN_CAN1_CS, PIN_CAN1_INT, [] { can1.mcp.isr(); });
CanBus can2("CAN2", "GT86", PIN_CAN2_CS, PIN_CAN2_INT, [] { can2.mcp.isr(); });

CanBus::CanBus(const char *name, const char *owner, uint8_t cs, uint8_t intPin, void (*isr)())
    : mcp(cs, SPI, intPin), name(name), owner(owner), isr(isr) {}

void canBegin() {
  for (uint8_t cs : {PIN_CAN1_CS, PIN_CAN2_CS}) {
    digitalWrite(cs, HIGH);
    pinMode(cs, OUTPUT);
  }
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  can1.begin();
  can2.begin();
}

bool CanBus::begin() {
  ACAN2515Settings settings(MCP2515_CRYSTAL_HZ, CAN_BITRATE);
  settings.mReceiveBufferSize = 32;
  settings.mTransmitBuffer0Size = 40;
  lastError = mcp.begin(settings, isr);
  ready = lastError == 0;
  if (ready) say("%s (%s): MCP2515 ok, 500 kbit/s, 8 MHz crystal", name, owner);
  else say("WARNING %s (%s): MCP2515 init failed, error 0x%04X. Needs barrel jack 2; retry with %s init.", name, owner, lastError, name);
  return ready;
}

void CanBus::loop() {
  if (!ready) return;
  CANMessage m;
  for (uint8_t i = 0; i < 8 && mcp.available(); i++) {
    mcp.receive(m);
    rxCount++;
    if (!sniff) continue;
    if (Serial.availableForWrite() < 64) {
      dropped++;
      continue;
    }
    char line[64];
    int n = snprintf(line, sizeof(line), "%s rx %03X [%u]", name, (unsigned)m.id, m.len);
    for (uint8_t b = 0; b < m.len && b < 8; b++) n += snprintf(line + n, sizeof(line) - n, " %02X", m.data[b]);
    say("%s", line);
  }
}

bool CanBus::send(uint32_t id, uint8_t len, const uint8_t *data) {
  if (!ready) return false;
  CANMessage m;
  m.id = id;
  m.ext = id > 0x7FF;
  m.len = len;
  memcpy(m.data, data, len);
  if (mcp.tryToSend(m)) {
    txCount++;
    fullWarned = false;
    return true;
  }
  txFail++;
  if (!fullWarned) {
    fullWarned = true;
    say("warning: %s TX queue full, no ACK? Check cluster power, wiring and termination.", name);
  }
  return false;
}

void CanBus::status() {
  if (!ready) {
    say("%s (%s): not initialised, error 0x%04X", name, owner, lastError);
    return;
  }
  say("%s (%s): ok, tx %u, tx fail %u, rx %u, sniff %s (%u dropped), TEC %u, REC %u, EFLG 0x%02X", name, owner,
      (unsigned)txCount, (unsigned)txFail, (unsigned)rxCount, sniff ? "on" : "off", (unsigned)dropped,
      mcp.transmitErrorCounter(), mcp.receiveErrorCounter(), mcp.errorFlagRegister());
}

void CanBus::command(int argc, char **argv) {
  if (argc < 2) {
    rej("use: status, sniff on|off, send <id> <bytes>, init");
  } else if (!strcmp(argv[1], "status")) {
    status();
  } else if (!strcmp(argv[1], "init")) {
    if (ready) say("%s already initialised", name);
    else begin();
  } else if (!strcmp(argv[1], "sniff")) {
    bool on;
    if (argc != 3 || !onOffArg(argv[2], on)) return;
    sniff = on;
    say("%s sniff %s", name, on ? "on" : "off");
  } else if (!strcmp(argv[1], "send")) {
    if (!ready) {
      rej("%s not initialised", name);
      return;
    }
    if (argc < 3 || argc > 11) {
      rej("use: send <id hex> <up to 8 hex bytes>");
      return;
    }
    char *end;
    const uint32_t id = strtoul(argv[2], &end, 16);
    if (*end || id > 0x1FFFFFFF) {
      rej("bad id");
      return;
    }
    // 0x380 = VIN, 0x600-0x6FF = diag/coding
    if (this == &can1 && (id == 0x380 || (id >= 0x600 && id <= 0x6FF))) {
      rej("refused on F10 bus: 0x380 and 0x600-0x6FF are blocked");
      return;
    }
    uint8_t data[8] = {};
    const uint8_t len = argc - 3;
    for (uint8_t i = 0; i < len; i++) {
      const unsigned long b = strtoul(argv[3 + i], &end, 16);
      if (*end || b > 0xFF) {
        rej("bad byte '%s'", argv[3 + i]);
        return;
      }
      data[i] = b;
    }
    if (send(id, len, data)) {
      say("%s tx %03X [%u]", name, (unsigned)id, len);
    } else {
      rej("TX queue full");
    }
  } else {
    rej("use: status, sniff on|off, send <id> <bytes>, init");
  }
}

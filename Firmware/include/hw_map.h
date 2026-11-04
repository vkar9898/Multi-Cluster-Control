#pragma once
#include <stdint.h>

constexpr uint8_t PIN_TPIC_SER  = 26;
constexpr uint8_t PIN_TPIC_CLK  = 4;
constexpr uint8_t PIN_TPIC_LAT  = 27;
constexpr uint8_t PIN_TPIC_OE   = 32;  // active low

constexpr uint8_t PIN_W126_TACH = 13;
constexpr uint8_t PIN_X90_TACH  = 33;
constexpr uint8_t PIN_W126_SPD  = 25;
constexpr uint8_t PIN_GT86_FUEL = 14;

constexpr uint8_t PIN_X27_STEP  = 17;
constexpr uint8_t PIN_X27_DIR   = 2;

constexpr uint8_t PIN_SPI_SCK   = 18;
constexpr uint8_t PIN_SPI_MOSI  = 23;
constexpr uint8_t PIN_SPI_MISO  = 19;
constexpr uint8_t PIN_CAN1_CS   = 5;
constexpr uint8_t PIN_CAN1_INT  = 34;
constexpr uint8_t PIN_CAN2_CS   = 15;
constexpr uint8_t PIN_CAN2_INT  = 35;

constexpr uint8_t PIN_I2C_SDA   = 21;
constexpr uint8_t PIN_I2C_SCL   = 22;
constexpr uint8_t PIN_LIGHT     = 36;

// even channels so each gets its own timer
constexpr uint8_t LEDC_W126_TACH = 0;
constexpr uint8_t LEDC_X90_TACH  = 2;
constexpr uint8_t LEDC_W126_SPD  = 4;
constexpr uint8_t LEDC_GT86_FUEL = 6;
constexpr uint32_t GT86_FUEL_HZ  = 31000;

constexpr uint32_t MCP2515_CRYSTAL_HZ = 8000000;
constexpr uint32_t CAN_BITRATE        = 500000;

constexpr uint8_t  PCA_ADDR       = 0x40;
constexpr uint16_t PCA_DEFAULT_HZ = 1000;
constexpr uint16_t PCA_MAX_HZ     = 1526;
constexpr uint16_t PCA_MIN_HZ     = 24;

enum PcaChannel : uint8_t {
  PCA_W126_TEMP = 0,
  PCA_W126_FUEL = 1,
  PCA_W126_OIL  = 2,  // inverted
  PCA_X90_TEMP  = 3,
  PCA_X90_FUEL  = 4,
  PCA_X90_ODOFF = 5,
};

constexpr uint32_t TURN_HALF_MS = 333;

// bit = chip * 8 + drain
enum TpicBit : uint8_t {
  W126_BATTERY    = 0,
  W126_BRAKEFLUID = 1,
  W126_PADWEAR    = 2,
  W126_COOLANT    = 3,
  W126_WASHER     = 4,
  W126_FUELRES    = 5,
  W126_OILLEVEL   = 6,  // inverted
  W126_ABS        = 7,
  W126_BULBFAIL   = 8,
  W126_ILL        = 9,
  W126_HB         = 10,
  W126_LEFT       = 11,
  W126_RIGHT      = 12,
  X90_ILL         = 13,  // current limit
  X90_HB          = 14,
  X90_BRAKE       = 15,
  X90_CEL         = 16,
  X90_OIL         = 17,
  X90_POWER       = 18,
  X90_RIGHT       = 19,
  X90_LEFT        = 20,
  X90_CHARGE      = 21,
  GT86_SECURITY   = 22,
  GT86_SEATBELT   = 23,
  GT86_RIGHT      = 24,
  GT86_LEFT       = 25,
  X90_SEATBELT    = 26,
  X90_4WD         = 27,
  GT86_TRIP       = 28,
  GT86_OIL        = 29,
  GT86_CHARGE     = 30,
  GT86_BRAKE      = 31,
};

struct TpicName { uint8_t bit; const char *name; };
static const TpicName TPIC_NAMES[32] = {
  {W126_BATTERY,    "W126 battery/charge lamp"},
  {W126_BRAKEFLUID, "W126 brake fluid/park brake lamp"},
  {W126_PADWEAR,    "W126 pad wear lamp"},
  {W126_COOLANT,    "W126 coolant level lamp"},
  {W126_WASHER,     "W126 washer fluid lamp"},
  {W126_FUELRES,    "W126 fuel reserve lamp"},
  {W126_OILLEVEL,   "W126 oil level lamp (inverted)"},
  {W126_ABS,        "W126 ABS lamp"},
  {W126_BULBFAIL,   "W126 bulb-failure lamp"},
  {W126_ILL,        "W126 illumination"},
  {W126_HB,         "W126 high beam"},
  {W126_LEFT,       "W126 left turn"},
  {W126_RIGHT,      "W126 right turn"},
  {X90_ILL,         "X-90 illumination"},
  {X90_HB,          "X-90 high beam"},
  {X90_BRAKE,       "X-90 brake"},
  {X90_CEL,         "X-90 check engine"},
  {X90_OIL,         "X-90 oil pressure"},
  {X90_POWER,       "X-90 power mode"},
  {X90_RIGHT,       "X-90 right turn"},
  {X90_LEFT,        "X-90 left turn"},
  {X90_CHARGE,      "X-90 charge"},
  {GT86_SECURITY,   "GT86 security lamp"},
  {GT86_SEATBELT,   "GT86 seatbelt lamp"},
  {GT86_RIGHT,      "GT86 right turn"},
  {GT86_LEFT,       "GT86 left turn"},
  {X90_SEATBELT,    "X-90 seat belt"},
  {X90_4WD,         "X-90 4WD"},
  {GT86_TRIP,       "GT86 TRIP button"},
  {GT86_OIL,        "GT86 oil lamp"},
  {GT86_CHARGE,     "GT86 charge lamp"},
  {GT86_BRAKE,      "GT86 brake lamp"},
};

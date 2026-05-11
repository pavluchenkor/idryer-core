#pragma once

// Severities: (enum_value, short_name)
#define ERRSEV_LIST(X)          \
  X(ERRSEV_INFO,     "INFO")    \
  X(ERRSEV_WARNING,  "WARN")    \
  X(ERRSEV_ERROR,    "ERROR")   \
  X(ERRSEV_CRITICAL, "CRIT")

// Sources: (enum_value, short_name)
// Universal across all idryer devices (iHeater, Storage, Dryer, etc.)
#define ERRSRC_LIST(X)                      \
  X(ERRSRC_CORE,    "CORE")                 \
  X(ERRSRC_HEATER,  "HEATER")               \
  X(ERRSRC_AIR,     "AIR")                  \
  X(ERRSRC_THERM,   "THERMISTOR")           \
  X(ERRSRC_SHT,     "SHT")                  \
  X(ERRSRC_SERVO,   "SERVO")                \
  X(ERRSRC_SCALE,   "SCALE")                \
  X(ERRSRC_RFID,    "RFID")                 \
  X(ERRSRC_LED,     "LED")                  \
  X(ERRSRC_LINK,    "LINK")

// Error codes: (enum_value, machine_name, human_text)
// Generic codes applicable to any source.
#define ERRCODE_LIST(X)                                                          \
  X(ERRC_OK,               "OK",                 "OK")                          \
  X(ERRC_SENSOR_INVALID,   "SENSOR_INVALID",     "Sensor reading invalid")      \
  X(ERRC_OUT_OF_RANGE,     "OUT_OF_RANGE",       "Value out of range")          \
  X(ERRC_SENSOR_SHORT,     "SENSOR_SHORT",       "Short circuit")               \
  X(ERRC_SENSOR_OPEN,      "SENSOR_OPEN",        "Open circuit")                \
  X(ERRC_NO_RESPONSE,      "NO_RESPONSE",        "No response")                 \
  X(ERRC_OVER_MAX,         "OVER_MAX",           "Value over maximum")          \
  X(ERRC_UNDER_MIN,        "UNDER_MIN",          "Value under minimum")         \
  X(ERRC_TIMEOUT,          "TIMEOUT",            "Operation timeout")           \
  X(ERRC_CONFIG_INVALID,   "CONFIG_INVALID",     "Invalid configuration")       \
  X(ERRC_STATE_CHANGE,     "STATE_CHANGE",       "State changed")               \
  X(ERRC_PROTOCOL_ERROR,   "PROTOCOL_ERROR",     "Protocol error")

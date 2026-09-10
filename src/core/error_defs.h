#pragma once

// Severities: (enum_value, short_name)
#define ERRSEV_LIST(X)          \
  X(ERRSEV_INFO,     "INFO")    \
  X(ERRSEV_WARNING,  "WARN")    \
  X(ERRSEV_ERROR,    "ERROR")   \
  X(ERRSEV_CRITICAL, "CRIT")

// Sources: (enum_value, short_name)
// Universal across all idryer devices (iHeater, Storage, Dryer, etc.)
//
// ПОРЯДОК ЗНАЧИМ: iDryerControllerV2 пишет source и code в EEPROM журнала
// ошибок числами (ErrLogRec). Существующие строки НЕ переставлять и НЕ
// удалять — старые записи начнут читаться с чужими источниками. Новое
// добавлять только в конец списка.
#define ERRSRC_LIST(X)                      \
  X(ERRSRC_CORE,    "CORE")                 \
  X(ERRSRC_HEATER,  "HEATER")               \
  X(ERRSRC_AIR,     "AIR")                  \
  X(ERRSRC_THERM,   "THERMISTOR")           /* зарезервировано: термистор нагревателя репортится под HEATER */ \
  X(ERRSRC_SHT,     "SHT")                  /* зарезервировано: датчик климата репортится под AIR */ \
  X(ERRSRC_SERVO,   "SERVO")                \
  X(ERRSRC_MODE,    "MODE")                 \
  X(ERRSRC_STORAGE, "STORAGE_MODE")         \
  X(ERRSRC_DRYING,  "DRYING_MODE")          \
  X(ERRSRC_PID,     "PID_AUTOTUNE")         \
  X(ERRSRC_UI,      "UI")                   \
  X(ERRSRC_LINK,    "LINK")                 \
  X(ERRSRC_SCALE,   "SCALE")                \
  X(ERRSRC_RFID,    "RFID")                 \
  X(ERRSRC_LED,     "LED")

// Error codes: (enum_value, machine_name, human_text)
// Generic codes applicable to any source.
#define ERRCODE_LIST(X)                                                          \
  X(ERRC_OK,               "OK",                 "OK")                          \
  X(ERRC_SENSOR_INVALID,   "SENSOR_INVALID",     "Sensor reading invalid")      \
  X(ERRC_OUT_OF_RANGE,     "OUT_OF_RANGE",       "Out of range")                \
  X(ERRC_SENSOR_SHORT,     "SENSOR_SHORT",       "Short circuit")               \
  X(ERRC_SENSOR_OPEN,      "SENSOR_OPEN",        "Open circuit")                \
  X(ERRC_NO_RESPONSE,      "NO_RESPONSE",        "No response")                 \
  X(ERRC_OVER_MAX,         "OVER_MAX",           "Value over maximum")          \
  X(ERRC_UNDER_MIN,        "UNDER_MIN",          "Value under minimum")         \
  X(ERRC_TIMEOUT,          "TIMEOUT",            "Operation timeout")           \
  X(ERRC_MODE_SWITCH_FAIL, "MODE_SWITCH_FAILED", "Mode switch failed")          \
  X(ERRC_AUTOTUNE_FAIL,    "AUTOTUNE_FAILED",    "PID autotune failed")         \
  X(ERRC_CONFIG_INVALID,   "CONFIG_INVALID",     "Invalid configuration")       \
  X(ERRC_STATE_CHANGE,     "ERRC_STATE_CHANGE",  "Mode state change")           \
  X(ERRC_PROTOCOL_VERSION, "PROTOCOL_VERSION",   "Protocol version mismatch")   \
  X(ERRC_PROTOCOL_ERROR,   "PROTOCOL_ERROR",     "Protocol error")              \
  X(ERRC_ABNORMAL_RESET,   "ABNORMAL_RESET",     "Device restarted abnormally") \
  X(ERRC_LOW_MEMORY,       "LOW_MEMORY",         "Free memory critically low")  \
  X(ERRC_AUTH_FAILED,      "AUTH_FAILED",        "Authentication rejected")

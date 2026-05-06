/**
 * @file idryer_core.h
 * @brief Core idryer device stack — main entry point.
 *
 * Include this header to get the full set of components needed for any
 * ESP32-based iDryer device: WiFi, HTTP, cloud state machine, MQTT, and runtime.
 *
 * Typical assembly (in your product's main.cpp):
 * @code
 * #include <idryer_core.h>
 *
 * idryer::ArduinoWifiStore       wifiStore;
 * idryer::ArduinoWifiManager     wifi;
 * idryer::ArduinoCredentialStore credentials;
 * idryer::ArduinoHttpClient      http;
 * idryer::cloud::HttpApi         api(&http, IDRYER_API_BASE);
 * idryer::MqttClient             mqtt;
 * idryer::cloud::CloudStateMachine cloud(&wifi, &credentials, &api, &mqtt);
 * idryer::ActionDispatcher       dispatcher;
 * idryer::IdryerRuntime          runtime(&cloud, &dispatcher, &profile, &mqtt);
 *
 * void setup() { runtime.begin(); }
 * void loop()  { runtime.loop();  }
 * @endcode
 *
 * Optional modules — include separately when needed:
 *   - @c <local_access/local_access.h> — LAN WebSocket server + mDNS discovery
 *   - @c <idryer_uart.h>               — UART bridge for two-MCU devices (ESP32 + RP2040)
 *   - @c <idryer_integrations.h>       — Home Assistant / Bambu Lab / Moonraker connectivity
 */

#pragma once

#include "core/config.h"
#include "core/types.h"
#include "hal/hal_types.h"
#include "hal/hal_arduino.h"
#include "device/interfaces/IWifiManager.h"
#include "device/interfaces/IHttpClient.h"
#include "device/interfaces/ICredentialStore.h"
#include "platform/arduino/ArduinoWifiManager.h"
#include "platform/arduino/ArduinoHttpClient.h"
#include "platform/arduino/ArduinoCredentialStore.h"
#include "platform/arduino/ArduinoWifiStore.h"
#include "mqtt/idryer_topics.h"
#include "mqtt/mqtt_client.h"
#include "cloud/http_api.h"
#include "cloud/cloud_state_machine.h"
#include "cloud/action_dispatcher.h"
#include "profiles/IProfile.h"
#include "runtime/idryer_runtime.h"

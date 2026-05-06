#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef IDRYER_MAX_SERIAL_NUMBER_LEN
#define IDRYER_MAX_SERIAL_NUMBER_LEN 32
#endif

#ifndef IDRYER_MAX_TOKEN_LEN
#define IDRYER_MAX_TOKEN_LEN 512
#endif

#ifndef IDRYER_MAX_DEVICE_ID_LEN
#define IDRYER_MAX_DEVICE_ID_LEN 40
#endif

#ifndef IDRYER_MAX_SSID_LEN
#define IDRYER_MAX_SSID_LEN 33
#endif

#ifndef IDRYER_MAX_PASSWORD_LEN
#define IDRYER_MAX_PASSWORD_LEN 64
#endif

#ifndef IDRYER_MAX_IP_LEN
#define IDRYER_MAX_IP_LEN 16
#endif

#ifndef IDRYER_MAX_MAC_LEN
#define IDRYER_MAX_MAC_LEN 18
#endif

#ifndef IDRYER_MAX_URL_LEN
#define IDRYER_MAX_URL_LEN 256
#endif

#ifndef IDRYER_MAX_PIN_LEN
#define IDRYER_MAX_PIN_LEN 9
#endif

#ifndef IDRYER_WIFI_RETRY_INTERVAL_MS
#define IDRYER_WIFI_RETRY_INTERVAL_MS 5000
#endif

#ifndef IDRYER_PROVISION_RETRY_MS
#define IDRYER_PROVISION_RETRY_MS 10000
#endif

#ifndef IDRYER_CLAIM_POLL_INTERVAL_MS
#define IDRYER_CLAIM_POLL_INTERVAL_MS 5000
#endif

#ifndef IDRYER_MQTT_RETRY_INTERVAL_MS
#define IDRYER_MQTT_RETRY_INTERVAL_MS 5000
#endif

static_assert(IDRYER_MAX_SERIAL_NUMBER_LEN >= 16, "Serial number buffer too small");
static_assert(IDRYER_MAX_TOKEN_LEN >= 128, "Token buffer too small for JWT");
static_assert(IDRYER_MAX_DEVICE_ID_LEN >= 36, "Device ID buffer too small for UUID");

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoHttpClient.h"
#include "../../hal/hal_types.h"
#include "../../mqtt/root_ca.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace idryer {

ArduinoHttpClient::ArduinoHttpClient() {}

bool ArduinoHttpClient::postJson(const char* url, const char* body, JsonDocument& response) {
    if (!url || !body) return false;

    HAL_LOG_DEBUG("HTTP", "POST %s", url);
    HAL_LOG_DEBUG("HTTP", "Body: %s", body);

    WiFiClientSecure client;
    client.setCACert(ROOT_CA_LETSENCRYPT);

    HTTPClient https;
    https.setTimeout(timeout_);

    if (!https.begin(client, url)) {
        HAL_LOG_ERROR("HTTP", "https.begin() FAILED for: %s", url);
        return false;
    }

    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST(body);

    if (httpCode < 200 || httpCode >= 300) {
        HAL_LOG_ERROR("HTTP", "POST %s failed: %d", url, httpCode);
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    DeserializationError err = deserializeJson(response, payload);
    if (err) {
        HAL_LOG_ERROR("HTTP", "JSON parse error: %s", err.c_str());
        return false;
    }
    return true;
}

bool ArduinoHttpClient::getJson(const char* url, JsonDocument& response) {
    if (!url) return false;

    HAL_LOG_DEBUG("HTTP", "GET %s", url);

    WiFiClientSecure client;
    client.setCACert(ROOT_CA_LETSENCRYPT);

    HTTPClient https;
    https.setTimeout(timeout_);

    if (!https.begin(client, url)) {
        HAL_LOG_ERROR("HTTP", "Failed to begin connection to: %s", url);
        return false;
    }

    int httpCode = https.GET();
    // 404 is valid for /check-claim (device not claimed yet)
    if (httpCode < 0 || (httpCode >= 300 && httpCode != 404)) {
        HAL_LOG_ERROR("HTTP", "GET %s failed: %d", url, httpCode);
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    DeserializationError err = deserializeJson(response, payload);
    if (err) {
        HAL_LOG_ERROR("HTTP", "JSON parse error: %s", err.c_str());
        return false;
    }
    return true;
}

void ArduinoHttpClient::setTimeout(uint32_t timeoutMs) { timeout_ = timeoutMs; }

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM

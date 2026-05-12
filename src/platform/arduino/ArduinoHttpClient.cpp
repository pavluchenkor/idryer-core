#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoHttpClient.h"
#include "../../hal/hal_types.h"
#include "../../mqtt/root_ca.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace idryer {

ArduinoHttpClient::ArduinoHttpClient() {}

static bool isHttps(const char* url) {
    return url && strncmp(url, "https://", 8) == 0;
}

bool ArduinoHttpClient::postJson(const char* url, const char* body, JsonDocument& response) {
    if (!url || !body) return false;

    HAL_LOG_DEBUG("HTTP", "POST %s", url);
    HAL_LOG_DEBUG("HTTP", "Body: %s", body);

    int httpCode;
    String payload;

    if (isHttps(url)) {
        WiFiClientSecure client;
        client.setCACert(ROOT_CA_LETSENCRYPT);
        HTTPClient http;
        http.setTimeout(timeout_);
        if (!http.begin(client, url)) {
            HAL_LOG_ERROR("HTTP", "http.begin() FAILED for: %s", url);
            return false;
        }
        http.addHeader("Content-Type", "application/json");
        httpCode = http.POST(body);
        if (httpCode >= 200 && httpCode < 300) payload = http.getString();
        http.end();
    } else {
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(timeout_);
        if (!http.begin(client, url)) {
            HAL_LOG_ERROR("HTTP", "http.begin() FAILED for: %s", url);
            return false;
        }
        http.addHeader("Content-Type", "application/json");
        httpCode = http.POST(body);
        if (httpCode >= 200 && httpCode < 300) payload = http.getString();
        http.end();
    }

    if (httpCode < 200 || httpCode >= 300) {
        HAL_LOG_ERROR("HTTP", "POST %s failed: %d", url, httpCode);
        return false;
    }

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

    int httpCode;
    String payload;

    if (isHttps(url)) {
        WiFiClientSecure client;
        client.setCACert(ROOT_CA_LETSENCRYPT);
        HTTPClient http;
        http.setTimeout(timeout_);
        if (!http.begin(client, url)) {
            HAL_LOG_ERROR("HTTP", "Failed to begin connection to: %s", url);
            return false;
        }
        httpCode = http.GET();
        if (httpCode >= 200 && httpCode < 300) payload = http.getString();
        http.end();
    } else {
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(timeout_);
        if (!http.begin(client, url)) {
            HAL_LOG_ERROR("HTTP", "Failed to begin connection to: %s", url);
            return false;
        }
        httpCode = http.GET();
        if (httpCode >= 200 && httpCode < 300) payload = http.getString();
        http.end();
    }

    // 404 = not yet claimed — caller handles empty result, no error log
    if (httpCode == 404) return false;
    if (httpCode < 0 || httpCode >= 300) {
        HAL_LOG_ERROR("HTTP", "GET %s failed: %d", url, httpCode);
        return false;
    }

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

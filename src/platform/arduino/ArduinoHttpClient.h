#pragma once

#include "../../device/interfaces/IHttpClient.h"

namespace idryer {

/**
 * @brief TLS-capable HTTP client for ESP32 (Arduino framework).
 *
 * Used by @c CloudStateMachine internally for provisioning and claim API calls.
 * You don't call this directly — pass it to @c HttpApi and let @c CloudStateMachine
 * use it.
 *
 * TLS: uses @c WiFiClientSecure with Let's Encrypt ISRG Root X1 CA certificate
 * (bundled in @c root_ca.h).
 */
class ArduinoHttpClient : public IHttpClient {
public:
    ArduinoHttpClient();

    /**
     * @brief Sends a POST request with a JSON body and parses the JSON response.
     * @param url      Full HTTPS URL.
     * @param body     Request body as a null-terminated JSON string.
     * @param response Output document to deserialize the response into.
     * @return @c true if the request succeeded and the response was valid JSON.
     */
    bool postJson(const char* url, const char* body, JsonDocument& response) override;

    /**
     * @brief Sends a GET request and parses the JSON response.
     * @param url      Full HTTPS URL.
     * @param response Output document to deserialize the response into.
     * @return @c true if the request succeeded and the response was valid JSON.
     */
    bool getJson(const char* url, JsonDocument& response) override;

    /**
     * @brief Sets the request timeout.
     * @param timeoutMs Timeout in milliseconds. Default is 10000 ms.
     */
    void setTimeout(uint32_t timeoutMs) override;

private:
    uint32_t timeout_ = 10000;
};

} // namespace idryer

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>

namespace idryer {

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    virtual bool postJson(const char* url, const char* body, JsonDocument& response) = 0;
    virtual bool getJson(const char* url, JsonDocument& response) = 0;
    virtual void setTimeout(uint32_t timeoutMs) = 0;
};

} // namespace idryer

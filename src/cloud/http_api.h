#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../device/interfaces/IHttpClient.h"

namespace idryer {
namespace cloud {

struct ProvisionResult {
    bool success;
    bool isNew;
    bool isClaimed;
    char token[IDRYER_MAX_TOKEN_LEN];
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN];

    ProvisionResult() : success(false), isNew(false), isClaimed(false) {
        token[0] = '\0'; deviceId[0] = '\0';
    }
};

struct RegisterResult {
    bool success;
    bool alreadyClaimed;
    char pin[IDRYER_MAX_PIN_LEN];
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN];
    uint32_t remainingSeconds;

    RegisterResult() : success(false), alreadyClaimed(false), remainingSeconds(0) {
        pin[0] = '\0'; deviceId[0] = '\0';
    }
};

struct ClaimCheckResult {
    bool success;
    bool claimed;
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN];

    ClaimCheckResult() : success(false), claimed(false) { deviceId[0] = '\0'; }
};

class HttpApi {
public:
    HttpApi(IHttpClient* http, const char* baseUrl);

    ProvisionResult  provision(const char* serialNumber);
    RegisterResult   registerDevice(const char* token, const char* serialNumber = nullptr);
    ClaimCheckResult checkClaim(const char* token);

private:
    IHttpClient* http_;
    char baseUrl_[IDRYER_MAX_URL_LEN];

    void buildUrl(char* buffer, size_t bufferSize, const char* path) const;
};

} // namespace cloud
} // namespace idryer

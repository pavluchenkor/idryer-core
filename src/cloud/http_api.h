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
    /// binding-v2: false = токен порталу неизвестен → идти в provision.
    /// Старый портал поля не шлёт — default true («жди дальше»), совместимо.
    bool known;
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN];

    ClaimCheckResult() : success(false), claimed(false), known(true) { deviceId[0] = '\0'; }
};

/// binding-v3: результат активации токеном привязки.
/// success → deviceToken заполнен (постоянный секрет). conflict → железо
/// привязано к другому аккаунту (портал вернул DEVICE_CONFLICT).
struct ActivateResult {
    bool success;
    bool conflict;
    bool rejected;  ///< Стабильный отказ портала (400: токен невалиден/истёк, лимит) — ретрай бессмыслен.
    char deviceToken[IDRYER_MAX_TOKEN_LEN];
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN];

    ActivateResult() : success(false), conflict(false), rejected(false) { deviceToken[0] = '\0'; deviceId[0] = '\0'; }
};

class HttpApi {
public:
    HttpApi(IHttpClient* http, const char* baseUrl);

    ProvisionResult  provision(const char* serialNumber);
    RegisterResult   registerDevice(const char* token, const char* serialNumber = nullptr);
    ClaimCheckResult checkClaim(const char* token);

    /// binding-v3: обменять токен привязки на постоянный секрет устройства.
    /// Устройство получает токен привязки локально (флешер/приложение) и
    /// вызывает activate; при успехе сохраняет deviceToken в NVS.
    ActivateResult   activate(const char* pairingToken, const char* serialNumber, const char* mcuSerial = nullptr);

private:
    IHttpClient* http_;
    char baseUrl_[IDRYER_MAX_URL_LEN];

    void buildUrl(char* buffer, size_t bufferSize, const char* path) const;
};

} // namespace cloud
} // namespace idryer

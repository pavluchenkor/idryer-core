#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../device/interfaces/IHttpClient.h"

namespace idryer {
namespace cloud {

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

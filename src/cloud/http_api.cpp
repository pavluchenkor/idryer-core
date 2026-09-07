#if defined(ESP32) || defined(ESP_PLATFORM)

#include "http_api.h"
#include "../hal/hal_types.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>

namespace idryer {
namespace cloud {

HttpApi::HttpApi(IHttpClient* http, const char* baseUrl) : http_(http) {
    if (baseUrl) {
        strncpy(baseUrl_, baseUrl, sizeof(baseUrl_) - 1);
        baseUrl_[sizeof(baseUrl_)-1] = '\0';
    } else {
        baseUrl_[0] = '\0';
    }
}

ActivateResult HttpApi::activate(const char* pairingToken, const char* serialNumber, const char* mcuSerial) {
    ActivateResult result;
    if (!http_ || !pairingToken || pairingToken[0] == '\0' || !serialNumber || serialNumber[0] == '\0') {
        HAL_LOG_ERROR("HTTP", "activate: invalid params");
        return result;
    }

    char url[IDRYER_MAX_URL_LEN];
    buildUrl(url, sizeof(url), "/devices/activate");

    DynamicJsonDocument body(256);
    body["pairingToken"] = pairingToken;
    body["serialNumber"] = serialNumber;
    if (mcuSerial && mcuSerial[0] != '\0') body["mcuSerial"] = mcuSerial;
    char payload[512];
    serializeJson(body, payload, sizeof(payload));

    HAL_LOG_INFO("HTTP", "POST %s", url);

    DynamicJsonDocument response(768);
    bool ok = http_->postJson(url, payload, response);

    // Успех — если пришёл постоянный секрет.
    if (ok && response.containsKey("deviceToken")) {
        const char* t = response["deviceToken"].as<const char*>();
        if (t) { strncpy(result.deviceToken, t, sizeof(result.deviceToken)-1); result.deviceToken[sizeof(result.deviceToken)-1] = '\0'; }
        if (response.containsKey("deviceId")) {
            const char* d = response["deviceId"].as<const char*>();
            if (d) { strncpy(result.deviceId, d, sizeof(result.deviceId)-1); result.deviceId[sizeof(result.deviceId)-1] = '\0'; }
        }
        result.success = (result.deviceToken[0] != '\0');
        HAL_LOG_INFO("HTTP", "activate OK: deviceId=%s", result.deviceId);
        return result;
    }

    // Ошибка: разбираем errorCode.
    //  - DEVICE_CONFLICT (409) — железо занято другим аккаунтом (обрабатывается
    //    отдельно, приложение показывает пользователю);
    //  - любой другой errorCode (400: INVALID_PAIRING_TOKEN / PAIRING_TOKEN_EXPIRED /
    //    DEVICE_LIMIT_REACHED) — СТАБИЛЬНЫЙ отказ: ретрай не поможет ни через минуту,
    //    ни через сутки. Помечаем rejected, чтобы cloud прекратил ретраи, стёр
    //    мёртвый токен и вернулся в SETUP (владелец подаст свежий токен).
    //  - НЕТ errorCode (нет тела) — сетевая/временная ошибка: ретраить корректно.
    if (response.containsKey("errorCode")) {
        const char* ec = response["errorCode"].as<const char*>();
        if (ec && strcmp(ec, "DEVICE_CONFLICT") == 0) {
            result.conflict = true;
            HAL_LOG_INFO("HTTP", "activate CONFLICT: hardware bound to another account");
            return result;
        }
        result.rejected = true;
        HAL_LOG_INFO("HTTP", "activate rejected (stable, no retry): %s", ec ? ec : "?");
    } else {
        // Тело без errorCode бывает и у 5xx портала — печатаем, что пришло:
        // иначе на железе видно только «failed», и причина теряется.
        char body[192] = {};
        serializeJson(response, body, sizeof(body));
        HAL_LOG_ERROR("HTTP", "activate failed (network/temporary — will retry), body=%s",
                      body[0] ? body : "<empty>");
    }
    return result;
}

void HttpApi::buildUrl(char* buffer, size_t bufferSize, const char* path) const {
    snprintf(buffer, bufferSize, "%s%s", baseUrl_, path);
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM

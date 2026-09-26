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

// TLS-рукопожатие с проверкой цепочки не помещается в стек главной задачи:
// Arduino даёт loopTask 8 КБ, а mbedTLS на разборе сертификата просит больше.
// Устройство падало с «stack overflow in task loopTask» ровно на привязке —
// единственном месте, где прошивка ходит по HTTPS синхронно. Поэтому запрос
// уносится в свою задачу с большим стеком, а вызывающий ждёт её завершения.
// Не меняет поведения вызова: тот же результат, тот же таймаут.
namespace {

constexpr uint32_t kTlsTaskStack = 16384;  // с запасом к замеренному пику
constexpr int      kTlsTaskPrio  = 5;      // как у loopTask, не вытесняем MQTT

struct TlsJob {
    const char*        url;
    const char*        body;      // nullptr → GET
    JsonDocument*      response;
    uint32_t           timeoutMs;
    bool               ok;
    int                httpCode;  // нужен вызывающему: 404 у GET = «ещё не привязано»
    SemaphoreHandle_t  done;
};

// Тело запроса: выполняется уже в задаче с достаточным стеком.
void tlsWorker(void* arg) {
    TlsJob* job = static_cast<TlsJob*>(arg);
    int httpCode = 0;
    String payload;

    {
        WiFiClientSecure client;
        client.setCACert(ROOT_CA_LETSENCRYPT);
        HTTPClient http;
        http.setTimeout(job->timeoutMs);
        if (http.begin(client, job->url)) {
            http.addHeader("Content-Type", "application/json");
            httpCode = job->body ? http.POST(job->body) : http.GET();
            if (httpCode > 0) payload = http.getString();
            http.end();
        } else {
            HAL_LOG_ERROR("HTTP", "http.begin() FAILED for: %s", job->url);
        }
    }

    job->ok = false;
    if (httpCode == 404 && !job->body) {
        // GET: «ещё не привязано» — не ошибка, вызывающий разберётся сам.
        job->httpCode = httpCode;
    } else if (httpCode < 200 || httpCode >= 300) {
        // Тело ошибки отдаём вызывающему: в 4xx/409 портал кладёт errorCode,
        // по которому cloud отличает стабильный отказ от временного сбоя.
        if (payload.length() > 0) deserializeJson(*job->response, payload);
        HAL_LOG_ERROR("HTTP", "%s %s failed: %d body=%s", job->body ? "POST" : "GET",
                      job->url, httpCode, payload.length() > 0 ? payload.c_str() : "<empty>");
    } else {
        DeserializationError err = deserializeJson(*job->response, payload);
        if (err) HAL_LOG_ERROR("HTTP", "JSON parse error: %s", err.c_str());
        else     job->ok = true;
    }
    job->httpCode = httpCode;

    xSemaphoreGive(job->done);
    vTaskDelete(nullptr);
}

// Запускает задачу и ждёт её. Не удалось создать — честно сообщаем, а не
// падаем: лучше «привязка не прошла, повторим», чем перезагрузка.
bool runTls(TlsJob& job) {
    job.done = xSemaphoreCreateBinary();
    if (!job.done) {
        HAL_LOG_ERROR("HTTP", "no semaphore for TLS task");
        return false;
    }
    TaskHandle_t h = nullptr;
    BaseType_t rc = xTaskCreate(tlsWorker, "idryer_tls", kTlsTaskStack, &job, kTlsTaskPrio, &h);
    if (rc != pdPASS) {
        HAL_LOG_ERROR("HTTP", "cannot start TLS task (heap?)");
        vSemaphoreDelete(job.done);
        return false;
    }
    // Ждём дольше сетевого таймаута: рукопожатие плюс чтение ответа.
    const TickType_t wait = pdMS_TO_TICKS(job.timeoutMs + 10000);
    if (xSemaphoreTake(job.done, wait) != pdTRUE) {
        HAL_LOG_ERROR("HTTP", "TLS task did not finish in time");
        vTaskDelete(h);
        vSemaphoreDelete(job.done);
        return false;
    }
    vSemaphoreDelete(job.done);
    return job.ok;
}

} // namespace

bool ArduinoHttpClient::postJson(const char* url, const char* body, JsonDocument& response) {
    if (!url || !body) return false;

    HAL_LOG_DEBUG("HTTP", "POST %s", url);
    HAL_LOG_DEBUG("HTTP", "Body: %s", body);

    // HTTPS — в отдельной задаче: стека loopTask не хватает на рукопожатие.
    if (isHttps(url)) {
        TlsJob job{ url, body, &response, timeout_, false, 0, nullptr };
        return runTls(job);
    }

    int httpCode;
    String payload;

    {
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(timeout_);
        if (!http.begin(client, url)) {
            HAL_LOG_ERROR("HTTP", "http.begin() FAILED for: %s", url);
            return false;
        }
        http.addHeader("Content-Type", "application/json");
        httpCode = http.POST(body);
        // Тело читаем при любом ответе: в 4xx/409 портал кладёт errorCode,
        // по которому cloud отличает стабильный отказ от временного сбоя.
        if (httpCode > 0) payload = http.getString();
        http.end();
    }

    if (httpCode < 200 || httpCode >= 300) {
        // Тело ошибки отдаётся вызывающему, если оно разобралось: без него
        // устройство не отличает «отказано» от «сеть моргнула».
        if (payload.length() > 0) deserializeJson(response, payload);
        HAL_LOG_ERROR("HTTP", "POST %s failed: %d body=%s", url, httpCode,
                      payload.length() > 0 ? payload.c_str() : "<empty>");
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

    // HTTPS — в отдельной задаче, см. комментарий к TlsJob.
    if (isHttps(url)) {
        TlsJob job{ url, nullptr, &response, timeout_, false, 0, nullptr };
        return runTls(job);
    }

    int httpCode;
    String payload;

    {
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(timeout_);
        if (!http.begin(client, url)) {
            HAL_LOG_ERROR("HTTP", "Failed to begin connection to: %s", url);
            return false;
        }
        httpCode = http.GET();
        // Тело читаем при любом ответе: в 4xx/409 портал кладёт errorCode,
        // по которому cloud отличает стабильный отказ от временного сбоя.
        if (httpCode > 0) payload = http.getString();
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

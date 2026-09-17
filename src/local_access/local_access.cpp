#if defined(ESP32) || defined(ESP_PLATFORM)

#include "local_access.h"
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <hal/hal_types.h>

#include <esp_heap_caps.h>

namespace idryer {

// ── WsImpl ───────────────────────────────────────────────────────────────────
// Subclass exposing protected sendFrame for fragmented delivery of large payloads
// without allocating a large intermediate buffer.

class LocalAccess::WsImpl : public WebSocketsServer {
public:
    explicit WsImpl(uint16_t port) : WebSocketsServer(port) {}

    /// Кадр целиком — одной записью в сокет.
    ///
    /// Каждая запись в TCP стоит около 1,8 КБ кучи на сегмент почти независимо
    /// от её размера: замер на стенде дал 1816 байт на префикс в 20 байт и
    /// 1796 байт на хвост в 1 байт. Посылка тремя фрагментами (sendFragmented)
    /// забирала так ~6 КБ на кусок меню в 700 байт, память не успевала
    /// вернуться к следующему куску, и на серии запросов конфига свободной
    /// кучи оставалось полторы тысячи байт, а передача обрывалась защитой
    /// low heap на середине — клиент получал обрубок меню.
    ///
    /// Заголовок пишется в зарезервированные WEBSOCKETS_MAX_HEADER_SIZE байт
    /// перед данными (headerToPayload): библиотека тогда не выделяет под кадр
    /// свою копию в куче (её путь «pack to one TCP package» вдобавок отключается
    /// сам, как только свободной кучи меньше 6 КБ, — ровно когда экономия нужнее
    /// всего) и делает ровно одну запись.
    ///
    /// Для клиента это то же самое сообщение: фрагментация — деталь транспорта,
    /// любая WS-библиотека собирает фрагменты в один текстовый кадр.
    ///
    /// @return false, если кадр не уместился в буфер — тогда зовите sendFragmented.
    bool sendWhole(uint8_t num,
                   const char* prefix, size_t prefixLen,
                   const char* json,   size_t jsonLen)
    {
        const size_t total = prefixLen + jsonLen + 1;   // +1 — закрывающая '}'
        if (total > kWholeFrameCap) return false;

        // Статический буфер, не стековый: путь сюда идёт из publishFull, где на
        // стеке уже лежат конверт куска и документ пункта меню.
        static uint8_t buf[WEBSOCKETS_MAX_HEADER_SIZE + kWholeFrameCap + 1];
        uint8_t* p = buf + WEBSOCKETS_MAX_HEADER_SIZE;
        memcpy(p, prefix, prefixLen);
        memcpy(p + prefixLen, json, jsonLen);
        p[prefixLen + jsonLen] = '}';
        p[total]               = '\0';   // отладочная печать библиотеки берёт %s

        return sendFrame(&_clients[num], WSop_text, buf, total,
                         /*fin=*/true, /*headerToPayload=*/true);
    }

    /// Запасной путь для кадров крупнее kWholeFrameCap: три фрагмента без
    /// промежуточного буфера. Дороже по куче — см. sendWhole.
    void sendFragmented(uint8_t num,
                        const char* prefix, size_t prefixLen,
                        const char* json,   size_t jsonLen)
    {
        WSclient_t* client = &_clients[num];
        char suffix = '}';
        sendFrame(client, WSop_text,
            reinterpret_cast<uint8_t*>(const_cast<char*>(prefix)), prefixLen, false);
        sendFrame(client, WSop_continuation,
            reinterpret_cast<uint8_t*>(const_cast<char*>(json)), jsonLen, false);
        sendFrame(client, WSop_continuation,
            reinterpret_cast<uint8_t*>(&suffix), 1, true);
    }

private:
    /// Хватает с запасом на всё, что уходит этим путём: кусок меню (конверт до
    /// 1120 байт), info (~600) и delta (~20).
    static constexpr size_t kWholeFrameCap = 2048;
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────

// Одно и то же объявление на всех путях: старт mDNS, привязка, REVOKE/WIPE.
// mdns_service_txt_item_set (внутри addServiceTxt) обновляет значение на лету,
// поэтому перезапускать сервис не нужно.
void LocalAccess::publishMdnsState()
{
    const char* state = deviceToken_[0] != '\0' ? "bound" : "setup";
    MDNS.addServiceTxt("_idryer", "_tcp", "state", state);
}

void LocalAccess::initMdns(const char* deviceName)
{
    if (!deviceName || deviceName[0] == '\0') {
        HAL_LOG_WARN("WS", "initMdns: deviceName empty");
        return;
    }
    strncpy(deviceName_, deviceName, sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';

    if (MDNS.begin(deviceName_)) {
        MDNS.addService("_idryer", "_tcp", 81);
        publishMdnsState();
        HAL_LOG_INFO("WS", "mDNS: %s.local → _idryer._tcp:81 (WS not yet started)", deviceName_);
    } else {
        HAL_LOG_WARN("WS", "mDNS: MDNS.begin failed for %s", deviceName_);
    }
}

void LocalAccess::begin(const char* deviceName, const char* deviceToken)
{
    if (enabled_) {
        HAL_LOG_WARN("WS", "begin: already enabled");
        return;
    }

    strncpy(deviceName_, deviceName ? deviceName : "", sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';
    strncpy(deviceToken_, deviceToken ? deviceToken : "", sizeof(deviceToken_) - 1);
    deviceToken_[sizeof(deviceToken_) - 1] = '\0';

    ws_ = new WsImpl(81);
    ws_->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        onWsEvent(num, static_cast<uint8_t>(type), payload, length);
    });
    ws_->begin();

    // Пинг клиента: у локального сервера лимит в одно соединение, а слот
    // освобождается только по событию отключения. Телефон, пропавший без
    // штатного закрытия (уснул, сменил сеть, приложение убили), оставлял
    // полуоткрытый сокет — устройство считало слот занятым и молча отбивало
    // всех новых, до перезагрузки по питанию.
    //
    // 1500 мс на ответ — с запасом не под сеть (она локальная), а под
    // собственный loop: во время публикации меню кусками он подвисает, и
    // слишком жёсткий таймаут отключал бы живого клиента. Два промаха подряд
    // — мёртвый уходит примерно за 13 секунд.
    ws_->enableHeartbeat(10000, 1500, 2);

    const bool mdnsOk = MDNS.begin(deviceName_);
    if (mdnsOk) {
        MDNS.addService("_idryer", "_tcp", 81);
        publishMdnsState();
    }

    enabled_ = true;
    HAL_LOG_INFO("WS", "Started: %s.local:81 mDNS=%s token=%s",
                 deviceName_,
                 mdnsOk ? "ok" : "fail",
                 deviceToken_[0] != '\0' ? "set" : "empty");
}

void LocalAccess::stop()
{
    if (!enabled_) return;
    if (ws_) { ws_->close(); delete ws_; ws_ = nullptr; }
    MDNS.end();
    enabled_         = false;
    connectedClient_ = -1;
    clientAuthorized_ = false;
    HAL_LOG_INFO("WS", "Stopped");
}

void LocalAccess::loop()
{
    if (!enabled_ || !ws_) return;
    ws_->loop();

    if (needsTokenRefresh_ && tokenRefreshCb_) {
        needsTokenRefresh_ = false;
        tokenRefreshCb_();
    }
}

bool LocalAccess::isListening() const
{
    return enabled_ && ws_ != nullptr;
}

void LocalAccess::updateToken(const char* newToken)
{
    if (!newToken || newToken[0] == '\0') return;
    strncpy(deviceToken_, newToken, sizeof(deviceToken_) - 1);
    deviceToken_[sizeof(deviceToken_) - 1] = '\0';
    publishMdnsState();
    HAL_LOG_INFO("WS", "Token updated");
}

// binding-v3: стереть токен после REVOKE/WIPE. Пустой deviceToken_ снова
// открывает неавторизованное окно пейринга (см. handleMessage type=="pair") без
// перезагрузки; текущего клиента, авторизованного старым токеном, разлогиниваем.
void LocalAccess::clearToken()
{
    deviceToken_[0]   = '\0';
    clientAuthorized_ = false;
    publishMdnsState();
    HAL_LOG_INFO("WS", "Token cleared — pairing window reopened (SETUP)");
}

// ── WS event dispatch ─────────────────────────────────────────────────────────

void LocalAccess::onWsEvent(uint8_t num, uint8_t type, uint8_t* payload, size_t length)
{
    WStype_t wsType = static_cast<WStype_t>(type);

    switch (wsType) {
    case WStype_CONNECTED:
        HAL_LOG_INFO("WS", ">>> CONNECTED #%d from %s", num, ws_->remoteIP(num).toString().c_str());
        if (connectedClient_ >= 0 && connectedClient_ != num) {
            HAL_LOG_WARN("WS", "Rejecting #%d — already have #%d", num, connectedClient_);
            // Называем причину до закрытия: молчаливый обрыв клиент не отличит
            // от отвергнутого токена и покажет человеку неверную ошибку.
            // Пишем прямо по номеру нового клиента — sendDoc адресует
            // connectedClient_, то есть того, кто слот и занимает.
            char busy[] = "{\"type\":\"auth_fail\",\"reason\":\"busy\"}";
            ws_->sendTXT(num, busy, sizeof(busy) - 1);
            ws_->disconnect(num);
            return;
        }
        connectedClient_  = num;
        clientAuthorized_ = false;
        break;

    case WStype_DISCONNECTED:
        HAL_LOG_INFO("WS", "<<< DISCONNECTED #%d (authorized=%d)", num, clientAuthorized_);
        if (connectedClient_ == num) {
            connectedClient_  = -1;
            clientAuthorized_ = false;
        }
        break;

    case WStype_TEXT:
        if (num == connectedClient_) {
            handleMessage(num, reinterpret_cast<const char*>(payload), length);
        }
        break;

    case WStype_ERROR:
        HAL_LOG_WARN("WS", "Error on client #%d", num);
        break;

    default:
        break;
    }
}

// ── Message handling ──────────────────────────────────────────────────────────
//
// WS message envelope:
//   incoming auth:    {"type":"auth","token":"<device_token>"}
//   incoming command: {"type":"command","command":"<name>","data":{...}}
//
// After unwrap, command and data are forwarded to CommandSink — the same
// handler the MQTT path uses. Transport envelope is never exposed to product.

void LocalAccess::handleMessage(uint8_t num, const char* json, size_t length)
{
    static StaticJsonDocument<1024> doc;
    doc.clear();
    if (deserializeJson(doc, json, length) != DeserializationError::Ok) {
        HAL_LOG_WARN("WS", "JSON parse error from #%d", num);
        return;
    }

    const char* type = doc["type"] | "";

    // ── Auth ──────────────────────────────────────────────────────────────────
    if (strcmp(type, "auth") == 0) {
        const char* token = doc["token"] | "";
        const bool  ok    = (deviceToken_[0] != '\0' && strcmp(token, deviceToken_) == 0);

        if (ok) {
            clientAuthorized_ = true;

            StaticJsonDocument<128> resp;
            resp["type"]       = "auth_ok";
            resp["deviceName"] = deviceName_;
            sendDoc(nullptr, resp);

            HAL_LOG_INFO("WS", "Auth OK — client #%d", num);

            // Immediately push current config + info so the app has a full
            // picture. info несёт mcuSerial (серийник RP2040 = идентичность на
            // портале) — без этого пуша поздно подключившийся LAN-клиент его не
            // получит (info шлётся только по событию, локальный WS не retained).
            if (commandSink_) {
                StaticJsonDocument<1> empty;
                commandSink_("get_config", empty.as<JsonObjectConst>());
                commandSink_("get_info", empty.as<JsonObjectConst>());
            }
        } else {
            StaticJsonDocument<64> resp;
            resp["type"]   = "auth_fail";
            resp["reason"] = "invalid_token";
            sendDoc(nullptr, resp);

            HAL_LOG_WARN("WS", "Auth failed — invalid token from #%d", num);
            needsTokenRefresh_ = true;
        }
        return;
    }

    // ── binding-v3: приём токена привязки (до привязки, БЕЗ auth) ──────────────
    // Приложение по локальному WS подаёт pairing-token свежему устройству
    // (постоянного секрета ещё нет). Это единственная операция без auth и
    // строго read-only: только принять токен. Уже привязанное устройство
    // (deviceToken есть) этот путь отклоняет — «на лету» не перепривязать.
    if (strcmp(type, "pair") == 0) {
        const char* ptoken = doc["token"] | "";
        StaticJsonDocument<64> resp;
        if (deviceToken_[0] == '\0' && ptoken[0] != '\0' && pairingTokenCb_) {
            pairingTokenCb_(ptoken);
            resp["type"] = "pair_ok";
            HAL_LOG_INFO("WS", "binding-v3: pairing token received via WS (#%d)", num);
        } else {
            resp["type"]   = "pair_fail";
            resp["reason"] = (deviceToken_[0] != '\0') ? "already_bound" : "no_token";
        }
        sendDoc(nullptr, resp);
        return;
    }

    // ── Require auth for all other messages ───────────────────────────────────
    if (!clientAuthorized_) {
        StaticJsonDocument<64> resp;
        resp["type"]   = "auth_fail";
        resp["reason"] = "not_authorized";
        sendDoc(nullptr, resp);
        return;
    }

    // ── Command ───────────────────────────────────────────────────────────────
    // Unwrap WS envelope and forward (command, data) to the shared CommandSink.
    // Transport envelope is stripped here — product sees only the command name
    // and its data, identical in shape to the MQTT command path.
    if (strcmp(type, "command") == 0) {
        const char*   command = doc["command"] | "";
        JsonObjectConst data  = doc["data"].as<JsonObjectConst>();

        if (command[0] != '\0' && commandSink_) {
            HAL_LOG_INFO("WS", "Command: %s", command);
            commandSink_(command, data);
        }
        return;
    }

    HAL_LOG_WARN("WS", "Unknown message type: %s", type);
}

// ── Outgoing data ─────────────────────────────────────────────────────────────

void LocalAccess::publish(const char* type, JsonDocument& doc)
{
    if (!isClientConnected()) return;
    sendDoc(type, doc);
}

void LocalAccess::publish(const char* type, const char* jsonRaw, size_t len)
{
    if (!isClientConnected() || !jsonRaw || len == 0) return;
    sendFragment(type, jsonRaw, len);
}

// Wraps doc as {"type":"...","data":{...}} and sends.
// When type == nullptr, sends doc as-is (used for auth responses).
void LocalAccess::sendDoc(const char* type, JsonDocument& doc)
{
    if (!ws_ || connectedClient_ < 0) return;

    if (type) {
        static StaticJsonDocument<2048> wrapper;
        static char buf[2048];
        wrapper.clear();
        wrapper["type"] = type;
        wrapper["data"] = doc.as<JsonObject>();
        const size_t len = serializeJson(wrapper, buf, sizeof(buf));
        HAL_LOG_INFO("WS", "TX type=%s %u bytes", type, static_cast<unsigned>(len));
        ws_->sendTXT(connectedClient_, buf, len);
    } else {
        static char buf[256];
        const size_t len = serializeJson(doc, buf, sizeof(buf));
        ws_->sendTXT(connectedClient_, buf, len);
    }
}

// Пороги темпа для sendFragment (см. комментарий внутри).
//   kTxPaceHeapRoom  — ниже этой отметки свободной кучи ждём слива сокета;
//                      взято с запасом над kMinHeapForConfig (12000), при
//                      котором передача меню обрывается совсем.
//   kTxPaceMaxWaitMs — потолок ожидания на один кадр: 28 кусков меню в худшем
//                      случае добавят полторы секунды, а не зависнут.
//   kTxPaceMinLen    — мелкие кадры (delta, ack) не пасём: они не копят очередь.
static constexpr uint32_t kTxPaceHeapRoom  = 20000;
static constexpr uint32_t kTxPaceMaxWaitMs = 60;
static constexpr size_t   kTxPaceMinLen    = 256;

// Wraps pre-serialized JSON as {"type":...,"data":<json>} and sends it as one
// WS frame (sendWhole). Falls back to a three-fragment send for oversized
// payloads. Pacing before the send — see the comment inside.
void LocalAccess::sendFragment(const char* type, const char* json, size_t len)
{
    if (!ws_ || connectedClient_ < 0) return;

    char prefix[48];
    const int prefixLen = snprintf(prefix, sizeof(prefix), "{\"type\":\"%s\",\"data\":", type);
    if (prefixLen <= 0 || static_cast<size_t>(prefixLen) >= sizeof(prefix)) {
        HAL_LOG_WARN("WS", "sendFragment: prefix overflow for type=%s", type);
        return;
    }

    // Темп передачи задаётся сливом сокета.
    //
    // Отданное в сокет живёт в куче, пока клиент не подтвердит приём. У
    // отправителя меню своего темпа нет: он отдаёт 28 кусков подряд так
    // быстро, как их принимает sendWhole. На первой передаче после
    // подключения (окно TCP ещё узкое) очередь росла быстрее, чем уходила в
    // сеть: свободной кучи оставалось около 3 КБ, и следующий запрос конфига
    // обрывался защитой low heap на втором куске — клиент получал обрубок.
    //
    // Поэтому перед крупным кадром ждём, пока сокет разгрузится. Ожидание
    // ограничено по времени: если клиент перестал читать совсем, кадр всё
    // равно уйдёт, а нехватку памяти поймает защита уровнем выше.
    if (len > kTxPaceMinLen) {
        const uint32_t started = HAL_MILLIS();
        while (heap_caps_get_free_size(MALLOC_CAP_DEFAULT) < kTxPaceHeapRoom &&
               HAL_MILLIS() - started < kTxPaceMaxWaitMs) {
            ws_->loop();
            delay(2);
        }
    }

    HAL_LOG_INFO("WS", "TX(raw) type=%s %u bytes", type, static_cast<unsigned>(len));
    if (!ws_->sendWhole(connectedClient_, prefix, static_cast<size_t>(prefixLen), json, len)) {
        ws_->sendFragmented(connectedClient_,
                            prefix, static_cast<size_t>(prefixLen),
                            json, len);
    }
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM

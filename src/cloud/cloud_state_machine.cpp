#if defined(ESP32) || defined(ESP_PLATFORM)

#include "cloud_state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "../radio_busy.h"
// Только ради BSSID/канала в логе подключения — см. ниже.
#include <WiFi.h>

namespace idryer {
namespace cloud {

const char* cloudStateToString(CloudState state) {
    switch (state) {
        case CloudState::Idle:                return "Idle";
        case CloudState::WifiConnecting:      return "WifiConnecting";
        case CloudState::WaitingForMcuSerial: return "WaitingForMcuSerial";
        case CloudState::Provisioning:        return "Provisioning";
        case CloudState::Registering:         return "Registering";
        case CloudState::Ready:               return "Ready";
        case CloudState::MqttConnecting:      return "MqttConnecting";
        case CloudState::Online:              return "Online";
        default:                              return "Unknown";
    }
}

CloudStateMachine::CloudStateMachine(IWifiManager* wifi, ICredentialStore* store,
                                     HttpApi* api, MqttClient* mqtt,
                                     const CloudConfig& config)
    : wifi_(wifi), store_(store), api_(api), mqtt_(mqtt), config_(config)
{
    mcuSerial_[0]          = '\0';
    mcuFirmwareVersion_[0] = '\0';
    mcuHardwareVersion_[0] = '\0';
    mqttKey_[0]            = '\0';
}

void CloudStateMachine::begin() {
    store_->begin();
    store_->load(identity_);

    HAL_LOG_INFO("CLOUD", "Init: serial=%s deviceId=%s",
                 identity_.hasSerialNumber() ? identity_.serialNumber : "(waiting)",
                 identity_.hasDeviceId()     ? identity_.deviceId     : "(none)");

    setState(CloudState::WifiConnecting);
    lastWifiAttempt_ = HAL_MILLIS() - config_.wifiRetryIntervalMs;
}

void CloudStateMachine::loop() {
    wifi_->loop();

    switch (state_) {
        case CloudState::WifiConnecting:      handleWifiConnecting();      break;
        case CloudState::WaitingForMcuSerial: handleWaitingForMcuSerial(); break;
        case CloudState::Provisioning:        handleProvisioning();        break;
        case CloudState::Ready:               handleReady();               break;
        case CloudState::MqttConnecting:      handleMqttConnecting();      break;
        case CloudState::Online:              handleOnline();              break;
        default: break;
    }
}

void CloudStateMachine::handleWifiConnecting() {
    if (wifi_->isConnected()) {
        char ip[IDRYER_MAX_IP_LEN];
        wifi_->getLocalIP(ip, sizeof(ip));
        // BSSID и канал — не формальность: в доме с несколькими точками на одном
        // SSID плата вольна выбрать не самую громкую, и тогда RSSI скачет от
        // включения к включению безо всякой связи с настройками радио. Без
        // адреса точки такие замеры сравнивать нельзя.
#if defined(ESP32) || defined(ESP_PLATFORM)
        HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm, AP: %s ch %d, SSID: %s",
                     ip, wifi_->getRSSI(), WiFi.BSSIDstr().c_str(), WiFi.channel(),
                     WiFi.SSID().c_str());
#else
        HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm", ip, wifi_->getRSSI());
#endif

        if (!identity_.hasSerialNumber() || (config_.waitForMcuSerial && !serialVerified_)) {
            setState(CloudState::WaitingForMcuSerial);
            return;
        }

        if (identity_.hasToken()) {
            // §4 модели v3: секрет в NVS — единственный источник правды о
            // привязке. Портал о привязке не переспрашивается (check-claim):
            // по одному такому ответу устройство стирало бы секрет, и откат
            // бэкапа или баг эндпоинта разом разваливал бы парк. Секрет
            // стирается ТОЛЬКО по явному REVOKE на командном
            // топике; карточка на портале не исчезает (soft-delete), а
            // REVOKING пускает устройство на брокер именно ради доставки
            // этой команды.
            setState(CloudState::Ready);
        } else {
            setState(CloudState::Provisioning);
        }
        return;
    }

    const uint32_t now = HAL_MILLIS();
    if (now - lastWifiAttempt_ < config_.wifiRetryIntervalMs) return;
    lastWifiAttempt_ = now;

    HAL_LOG_INFO("CLOUD", "Connecting to WiFi...");
    wifi_->connect();
}

void CloudStateMachine::handleWaitingForMcuSerial() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    if (serialVerified_) {
        lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
        setState(CloudState::Provisioning);
    }
}

void CloudStateMachine::setPairingToken(const char* token) {
    if (!token || token[0] == '\0') return;
    strncpy(pendingPairingToken_, token, sizeof(pendingPairingToken_) - 1);
    pendingPairingToken_[sizeof(pendingPairingToken_) - 1] = '\0';
    lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs; // активировать сразу
    HAL_LOG_INFO("CLOUD", "binding-v3: pairing token received (%d chars)", (int)strlen(pendingPairingToken_));
}

void CloudStateMachine::tryActivate() {
    if (pendingPairingToken_[0] == '\0') return;

    const uint32_t now = HAL_MILLIS();
    if (now - lastProvisionAttempt_ < config_.provisionRetryMs) return;
    lastProvisionAttempt_ = now;

    const char* mcu = (mcuSerial_[0] != '\0') ? mcuSerial_ : nullptr;
    HAL_LOG_INFO("CLOUD", "binding-v3: activating with pairing token (serial=%s mcu=%s)",
                 identity_.serialNumber, mcu ? mcu : "-");
    // Привязка — синхронный HTTPS к порталу: DNS, TCP, TLS-рукопожатие. Скан
    // эфира, затеянный ради выбора точки получше, уводит радио с канала на
    // пару секунд и рвёт рукопожатие — привязка срывается на ровном месте.
    // Отметка снимается сама на выходе из блока, включая все ранние возвраты
    // ниже по функции.
    ActivateResult r;
    {
        idryer::RadioBusyGuard radioGuard;
        r = api_->activate(pendingPairingToken_, identity_.serialNumber, mcu);
    }

    if (r.success && r.deviceToken[0] != '\0') {
        identity_.setToken(r.deviceToken);
        if (r.deviceId[0] != '\0') identity_.setDeviceId(r.deviceId);
        store_->save(identity_);
        pendingPairingToken_[0] = '\0'; // потреблён
        HAL_LOG_INFO("CLOUD", "binding-v3: activated, deviceId=%s -> Ready", identity_.deviceId);
        emitDiagnostic2("pair: activated deviceId=", identity_.deviceId);
        if (claimCompleteCallback_) claimCompleteCallback_(identity_.deviceId, claimCompleteCtx_);
        setState(CloudState::Ready);
        return;
    }
    if (r.conflict) {
        HAL_LOG_WARN("CLOUD", "binding-v3: CONFLICT — hardware bound to another account (prev owner must unbind)");
        pendingPairingToken_[0] = '\0'; // этот токен бесполезен, ждём новый
        return;
    }
    if (r.rejected) {
        // Стабильный отказ портала (токен невалиден/истёк, лимит): ретрай не
        // поможет никогда. Стираем мёртвый токен и остаёмся в ожидании нового
        // (SETUP, окно пейринга открыто) — иначе устройство бесконечно долбит
        // портал 400-ответами, а привязка молча не появляется.
        HAL_LOG_WARN("CLOUD", "binding-v3: pairing token rejected (dead/expired) — dropping, awaiting fresh token");
        pendingPairingToken_[0] = '\0';
        return;
    }
    HAL_LOG_WARN("CLOUD", "binding-v3: activate failed (network/temporary), will retry");
}

void CloudStateMachine::handleRevoke() {
    HAL_LOG_WARN("CLOUD", "binding-v3: REVOKE received — wiping secret, back to pairing (SETUP)");
    // Рапорт порталу «получил, стираю» — строго ДО стирания секрета и разрыва
    // сессии: после них публиковать нечем. Даём стеку короткое окно отправить
    // пакет; QoS1, брокер подтвердит. Если сессии нет (WIPE по проводу без
    // сети) — молча пропускаем, портал закроет карточку при следующем выходе.
    if (mqtt_ && mqtt_->isConnected()) {
        if (mqtt_->publishRevokeAck()) {
            const uint32_t until = HAL_MILLIS() + 400;
            while (HAL_MILLIS() < until) { mqtt_->loop(); HAL_DELAY_MS(10); }
            HAL_LOG_INFO("CLOUD", "binding-v3: revoke_ack sent");
        } else {
            HAL_LOG_WARN("CLOUD", "binding-v3: revoke_ack publish failed");
        }
    }
    identity_.token[0]        = '\0';
    identity_.deviceId[0]     = '\0';
    identity_.boundMqttKey[0] = '\0';
    store_->save(identity_);
    pendingPairingToken_[0] = '\0';
    unclaimedNotified_ = false;
    // Разорвать текущую MQTT-сессию: секрета больше нет, оставаться на брокере
    // нельзя. Сессия persistent (clean_session=false) и была поднята ДО стирания
    // секрета — без явного disconnect прошивка продолжала бы публиковать
    // телеметрию на старом коннекте, и портал видел бы ЛОЖНЫЙ online у
    // устройства, которое на деле уже в SETUP. Сбрасываем и флаг инициализации,
    // чтобы при следующей привязке MQTT поднялся заново на новом секрете.
    if (mqtt_) mqtt_->disconnect();
    mqttInitialized_ = false;
    // Вернуться к ожиданию токена привязки. handleProvisioning (v3) увидит
    // отсутствие секрета и будет ждать локальной подачи PAIR_TOKEN.
    setState(CloudState::Provisioning);
}

// binding-v3: чистый путь рождения. Секрет и deviceId появляются ТОЛЬКО через
// активацию токеном привязки (tryActivate). provision-по-MAC и PIN убраны:
// пока секрета нет и токен не подан — устройство просто ЖДЁТ (SETUP).
void CloudStateMachine::handleProvisioning() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }

    // Уже активировано (секрет + deviceId в NVS) → в MQTT.
    if (identity_.hasToken() && identity_.hasDeviceId()) {
        setState(CloudState::Ready);
        return;
    }

    // Подан токен привязки → активируемся им.
    if (pendingPairingToken_[0] != '\0') {
        tryActivate();
        return;
    }

    // Иначе ждём локальной подачи токена привязки (флешер/приложение).
    if (!unclaimedNotified_) {
        unclaimedNotified_ = true;
        HAL_LOG_INFO("CLOUD", "binding-v3: no secret — awaiting pairing token (SETUP)");
        if (unclaimedCallback_) unclaimedCallback_(unclaimedCtx_);
    }
}

void CloudStateMachine::handleReady() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    if (!identity_.hasToken() || !identity_.hasDeviceId()) { setState(CloudState::Provisioning); return; }
    setState(CloudState::MqttConnecting);
}

void CloudStateMachine::handleMqttConnecting() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }

    // espMqttClient асинхронный: connect() лишь инициирует, а TCP/TLS/MQTT
    // handshake продвигается внутри mqtt_->loop() — качаем его каждый тик.
    if (mqttInitialized_) mqtt_->loop();

    // Окно рукопожатия закрыто ограниченное время, а не до самого успеха.
    // Держать его на всю паузу backoff (до mqttRetryMaxMs) нельзя: пока
    // брокер недоступен, устройство неделями оставалось бы с закрытым радио и
    // ни разу не осмотрело бы эфир. А не осмотрев — не ушло бы с плохой точки,
    // из-за которой брокер и недоступен. Поэтому окно живёт только пока идёт
    // сама попытка; в паузах между попытками радио свободно.
    if (mqttRadioHeld_ && HAL_MILLIS() - mqttRadioHeldMs_ >= kMqttHandshakeGuardMs)
        releaseMqttRadio();

    if (mqtt_->isConnected()) {
        HAL_LOG_INFO("CLOUD", "MQTT connected!");
        mqttRetryCurrentMs_ = config_.mqttRetryIntervalMs; // сброс backoff
        authRejectStreak_ = 0; // binding-v2: успешный вход сбрасывает счётчик отказов
        setState(CloudState::Online);
        return;
    }

    const uint32_t now = HAL_MILLIS();
    if (mqttRetryCurrentMs_ == 0) mqttRetryCurrentMs_ = config_.mqttRetryIntervalMs;
    if (now - lastMqttAttempt_ < mqttRetryCurrentMs_) return;
    lastMqttAttempt_ = now;

    // binding-v2, самолечение: явный отказ авторизации (bad credentials=4 /
    // not authorized=5) три ретрая подряд → перевыпустить токен через
    // provision и заново пройти check-claim. Сетевые причины (TCP, TLS,
    // сервер недоступен) счётчик сбрасывают — они не отказ.
    // §4 модели v3: устройство НЕ гадает по отказам авторизации. Секрет
    // стирается только по явному REVOKE от портала. Отказ брокера — это или
    // временный сбой (деплой, перезапуск go-auth), или уже висящий REVOKE,
    // который придёт на командный топик: REVOKING пускает устройство на вход
    // именно ради доставки. Поэтому здесь только backoff и повтор.
    if (mqttInitialized_) {
        const uint8_t reason = mqtt_->lastDisconnectReason();
        if ((reason == 4 || reason == 5) && authRejectStreak_ < 255) {
            if (++authRejectStreak_ % 10 == 0) {
                HAL_LOG_WARN("CLOUD", "MQTT auth rejected x%u — retrying (secret is cleared only by REVOKE)",
                             authRejectStreak_);
            }
        } else if (reason != 4 && reason != 5) {
            authRejectStreak_ = 0;
        }
    }

    HAL_LOG_INFO("CLOUD", "Connecting to MQTT (retry in %us)...",
                 (unsigned)(mqttRetryCurrentMs_ / 1000));

    // Экспоненциальный backoff: каждая попытка = DNS + TCP/TLS handshake.
    // Недоступный брокер / auth-отказ (в т.ч. ACL-бан) не должен давать
    // молотилку раз в 5с часами — удваиваем паузу до потолка mqttRetryMaxMs.
    mqttRetryCurrentMs_ *= 2;
    if (mqttRetryCurrentMs_ > config_.mqttRetryMaxMs) mqttRetryCurrentMs_ = config_.mqttRetryMaxMs;

    if (!mqttInitialized_) {
        const char* key = getMqttKey(); // binding-v3: deviceId (UUID); legacy fallback внутри
        mqtt_->begin(key, identity_.token);
        mqttInitialized_ = true;
    }

    // Дальше — DNS, TCP, TLS-рукопожатие и MQTT CONNECT. Скан эфира, уводящий
    // радио с канала на пару секунд, срывает попытку и удваивает паузу до
    // следующей: одно неудачное совпадение уводит устройство в офлайн на
    // минуты. Отметку ставим перед попыткой и снимаем по успеху, по уходу из
    // состояния или по таймауту окна выше.
    if (!mqttRadioHeld_) {
        idryer::radioBusyBegin();
        mqttRadioHeld_   = true;
        mqttRadioHeldMs_ = HAL_MILLIS();
    }
    // Диагностика heap перед mbedtls handshake — включать при подозрениях на
    // фрагментацию .bss, ломающую TLS.
    // HAL_LOG_INFO("CLOUD", "heap before MQTT connect: free=%u largest=%u",
    //              (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
    //              (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    mqtt_->connect();
}

void CloudStateMachine::handleOnline() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    if (!mqtt_->isConnected()) { setState(CloudState::MqttConnecting); return; }
    mqtt_->loop();
}

// binding-v3: клейм убран целиком — и запрос PIN (requestClaim), и опрос
// check-claim. Привязку начинает владелец из приложения/флешера, устройство
// только принимает токен; отвязку — портал командой REVOKE.

idryer::McuSerialResult CloudStateMachine::setMcuSerial(const char* mcuSerial) {
    if (!mcuSerial || mcuSerial[0] == '\0') {
        return idryer::McuSerialResult::Ignored;
    }

    if (!identity_.hasBoundMqttKey()) {
        // First bind: no prior bound key — accept and use linkSerial as mqttKey
        strncpy(mcuSerial_, mcuSerial, sizeof(mcuSerial_) - 1);
        mcuSerial_[sizeof(mcuSerial_) - 1] = '\0';
        strncpy(mqttKey_, identity_.serialNumber, sizeof(mqttKey_) - 1);
        mqttKey_[sizeof(mqttKey_) - 1] = '\0';
        bindSwitchPending_ = true;
        serialVerified_ = true;
        HAL_LOG_INFO("CLOUD", "mcuSerial accepted (first bind): linkSerial=%s mcuSerial=%s",
                     identity_.serialNumber, mcuSerial_);
        if (state_ == CloudState::WaitingForMcuSerial) {
            lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
            setState(CloudState::Provisioning);
        }
        return idryer::McuSerialResult::AcceptedFirstBind;
    }

    if (strcmp(identity_.boundMqttKey, mcuSerial) == 0) {
        // Already bound: same RP2040 — use boundMqttKey as mqttKey
        strncpy(mcuSerial_, mcuSerial, sizeof(mcuSerial_) - 1);
        mcuSerial_[sizeof(mcuSerial_) - 1] = '\0';
        strncpy(mqttKey_, identity_.boundMqttKey, sizeof(mqttKey_) - 1);
        mqttKey_[sizeof(mqttKey_) - 1] = '\0';
        bindSwitchPending_ = false;
        serialVerified_ = true;
        HAL_LOG_INFO("CLOUD", "mcuSerial accepted (already bound): mqttKey=%s", mqttKey_);
        if (state_ == CloudState::WaitingForMcuSerial) {
            lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
            setState(CloudState::Provisioning);
        }
        return idryer::McuSerialResult::AcceptedBound;
    }

    HAL_LOG_WARN("CLOUD", "mcuSerial MISMATCH: bound=%s uart=%s",
                 identity_.boundMqttKey, mcuSerial);
    return idryer::McuSerialResult::Mismatch;
}

bool CloudStateMachine::handleBindAck(const char* mqttTopicKey, const char* mcuSerial) {
    if (!mqttTopicKey || !mcuSerial) return false;

    // One-ID: mqttTopicKey matches linkSerial — no MQTT switch needed
    if (strcmp(mqttTopicKey, identity_.serialNumber) == 0) {
        HAL_LOG_INFO("CLOUD", "bind_ack accepted (one-ID): topic unchanged");
        bindSwitchPending_ = false;
        return true;
    }

    // Two-chip: verify against mcuSerial_ we received in UART Hello
    if (mcuSerial_[0] == '\0' ||
        strcmp(mqttTopicKey, mcuSerial_) != 0 ||
        strcmp(mcuSerial,    mcuSerial_) != 0) {
        HAL_LOG_WARN("CLOUD", "bind_ack rejected: expected=%s got mqttTopicKey=%s mcuSerial=%s",
                     mcuSerial_, mqttTopicKey, mcuSerial);
        return false;
    }

    // Save boundMqttKey to NVS and switch MQTT identity
    identity_.setBoundMqttKey(mqttTopicKey);
    store_->save(identity_);

    strncpy(mqttKey_, mqttTopicKey, sizeof(mqttKey_) - 1);
    mqttKey_[sizeof(mqttKey_) - 1] = '\0';
    bindSwitchPending_ = false;

    HAL_LOG_INFO("CLOUD", "bind_ack accepted: switching MQTT to mqttKey=%s", mqttKey_);

    if (mqtt_->isConnected()) mqtt_->disconnect();
    mqttInitialized_ = false;
    mqttRetryCurrentMs_ = 0; // смена identity — backoff с чистого листа
    setState(CloudState::MqttConnecting);
    return true;
}

const char* CloudStateMachine::getMcuSerial() const {
    return (mcuSerial_[0] != '\0') ? mcuSerial_ : nullptr;
}

void CloudStateMachine::setMcuFirmwareVersion(uint32_t fwVersion) {
    snprintf(mcuFirmwareVersion_, sizeof(mcuFirmwareVersion_), "%u.%u.%u",
             (fwVersion >> 16) & 0xFF,
             (fwVersion >> 8)  & 0xFF,
             fwVersion         & 0xFF);
}

const char* CloudStateMachine::getMcuFirmwareVersion() const {
    return (mcuFirmwareVersion_[0] != '\0') ? mcuFirmwareVersion_ : nullptr;
}

void CloudStateMachine::setMcuHardwareVersion(const char* hwVersion) {
    if (!hwVersion) { mcuHardwareVersion_[0] = '\0'; return; }
    strncpy(mcuHardwareVersion_, hwVersion, sizeof(mcuHardwareVersion_) - 1);
    mcuHardwareVersion_[sizeof(mcuHardwareVersion_) - 1] = '\0';
}

void CloudStateMachine::setMcuWorkTimeCounter(uint32_t seconds) {
    mcuWorkTimeCounter_ = seconds;
}

uint32_t CloudStateMachine::getMcuWorkTimeCounter() const {
    return mcuWorkTimeCounter_;
}

const char* CloudStateMachine::getMcuHardwareVersion() const {
    return (mcuHardwareVersion_[0] != '\0') ? mcuHardwareVersion_ : nullptr;
}

const char* CloudStateMachine::getMqttKey() const {
    // binding-v3 UUID-адресация: MQTT-ключ (логин/топики) = deviceId — единый
    // для одночипа и двухчипа. Fallback (до активации / legacy-сборки):
    // mqttKey_ (из bind) или serialNumber.
    if (identity_.hasDeviceId()) return identity_.deviceId;
    return (mqttKey_[0] != '\0') ? mqttKey_ : identity_.serialNumber;
}

// Снять отметку «идёт рукопожатие с брокером», если она стоит. Отдельной
// функцией, потому что снимать её приходится из нескольких мест: по успеху, по
// уходу из состояния и по таймауту окна.
void CloudStateMachine::releaseMqttRadio() {
    if (!mqttRadioHeld_) return;
    idryer::radioBusyEnd();
    mqttRadioHeld_ = false;
}

void CloudStateMachine::setState(CloudState newState) {
    if (state_ == newState) return;
    CloudState oldState = state_;
    // Уходим из попытки подключения — окно закрыто в любом случае, чем бы она
    // ни кончилась. Без этого счётчик занятости остался бы поднятым навсегда.
    if (oldState == CloudState::MqttConnecting) releaseMqttRadio();
    state_ = newState;
    HAL_LOG_INFO("CLOUD", "State: %s -> %s", cloudStateToString(oldState), cloudStateToString(newState));
    if (stateCallback_) stateCallback_(oldState, newState, stateCallbackCtx_);
}

void CloudStateMachine::setStateChangeCallback(CloudStateChangeCallback cb, void* ctx) {
    stateCallback_ = cb; stateCallbackCtx_ = ctx;
}
void CloudStateMachine::setClaimCompleteCallback(ClaimCompleteCallback cb, void* ctx) {
    claimCompleteCallback_ = cb; claimCompleteCtx_ = ctx;
}
void CloudStateMachine::setUnclaimedCallback(UnclaimedCallback cb, void* ctx) {
    unclaimedCallback_ = cb; unclaimedCtx_ = ctx;
}

void CloudStateMachine::setDiagnosticCallback(DiagnosticCallback cb, void* ctx) {
    diagnosticCallback_ = cb; diagnosticCtx_ = ctx;
}

void CloudStateMachine::emitDiagnostic(const char* message) {
    if (diagnosticCallback_ && message) diagnosticCallback_(message, diagnosticCtx_);
}

void CloudStateMachine::emitDiagnostic2(const char* prefix, const char* value) {
    if (!diagnosticCallback_ || !prefix) return;
    char line[128];
    snprintf(line, sizeof(line), "%s%s", prefix, value ? value : "");
    diagnosticCallback_(line, diagnosticCtx_);
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM

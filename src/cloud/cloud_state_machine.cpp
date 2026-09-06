#if defined(ESP32) || defined(ESP_PLATFORM)

#include "cloud_state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

namespace idryer {
namespace cloud {

const char* cloudStateToString(CloudState state) {
    switch (state) {
        case CloudState::Idle:                return "Idle";
        case CloudState::WifiConnecting:      return "WifiConnecting";
        case CloudState::WaitingForMcuSerial: return "WaitingForMcuSerial";
        case CloudState::Provisioning:        return "Provisioning";
        case CloudState::Registering:         return "Registering";
        case CloudState::AwaitingClaim:       return "AwaitingClaim";
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
    pendingPin_[0]         = '\0';
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
        case CloudState::AwaitingClaim:       handleAwaitingClaim();       break;
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
        HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm", ip, wifi_->getRSSI());

        if (!identity_.hasSerialNumber() || (config_.waitForMcuSerial && !serialVerified_)) {
            setState(CloudState::WaitingForMcuSerial);
            return;
        }

        if (identity_.hasToken()) {
            // binding-v2: deviceId в NVS больше не даёт прыжка в Ready мимо
            // портала. Всегда сначала check-claim (AwaitingClaim сам решит:
            // подтверждено → Ready; «не привязан» → стереть deviceId и ждать
            // PIN; «токен не подходит» → provision). Это закрывает вечный
            // круг «запись удалили, пока модуль был офлайн».
            setState(CloudState::AwaitingClaim);
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
    ActivateResult r = api_->activate(pendingPairingToken_, identity_.serialNumber, mcu);

    if (r.success && r.deviceToken[0] != '\0') {
        identity_.setToken(r.deviceToken);
        if (r.deviceId[0] != '\0') identity_.setDeviceId(r.deviceId);
        store_->save(identity_);
        pendingPairingToken_[0] = '\0'; // потреблён
        HAL_LOG_INFO("CLOUD", "binding-v3: activated, deviceId=%s -> Ready", identity_.deviceId);
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

void CloudStateMachine::handleAwaitingClaim() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); awaitingClaim_ = false; return; }
    // binding-v2: deviceId в NVS сам по себе в Ready не пускает — привязку
    // подтверждает check-claim (ответ портала главнее памяти ESP).

    const uint32_t now = HAL_MILLIS();
    if (now - lastClaimPoll_ < config_.claimPollIntervalMs) return;
    lastClaimPoll_ = now;

    emitDiagnostic("claim: checking backend");
    ClaimCheckResult result = api_->checkClaim(identity_.token);
    if (!result.success) return; // сеть/5xx — не отказ, ничего не трогаем

    if (result.claimed) {
        if (result.deviceId[0] != '\0' &&
            strcmp(result.deviceId, identity_.deviceId) != 0) {
            identity_.setDeviceId(result.deviceId);
            store_->save(identity_);
        }
        awaitingClaim_ = false;
        HAL_LOG_INFO("CLOUD", "Device claimed! deviceId=%s", identity_.deviceId);
        emitDiagnostic2("claim: backend confirmed deviceId=", identity_.deviceId);
        if (claimCompleteCallback_) claimCompleteCallback_(identity_.deviceId, claimCompleteCtx_);
        setState(CloudState::Ready);
        return;
    }

    // claimed:false, известный токен — привязки на портале нет.
    if (result.known) {
        if (identity_.hasDeviceId()) {
            // binding-v2 mirror: запись удалили / портал из бэкапа. Стереть
            // deviceId + boundMqttKey, токен оставить — дальше обычный PIN.
            HAL_LOG_WARN("CLOUD", "Backend says not claimed — clearing stale deviceId=%s", identity_.deviceId);
            emitDiagnostic("claim: backend not claimed, clearing stale NVS binding");
            identity_.setDeviceId("");
            identity_.setBoundMqttKey("");
            store_->save(identity_);
            mqttKey_[0] = '\0';
            bindSwitchPending_ = false;
        }
        return; // ждём ввода PIN (человек)
    }

    // claimed:false + known:false — токен порталу неизвестен: стереть его и
    // провижиниться заново. Единственное осознанное стирание токена — по
    // явному сигналу портала, не по пустому ответу.
    HAL_LOG_WARN("CLOUD", "Backend does not know this token — re-provisioning");
    emitDiagnostic("claim: token unknown, re-provisioning");
    identity_.setToken("");
    identity_.setDeviceId("");
    identity_.setBoundMqttKey("");
    store_->save(identity_);
    mqttKey_[0] = '\0';
    bindSwitchPending_ = false;
    awaitingClaim_ = false;
    lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
    setState(CloudState::Provisioning);
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
    if (mqttInitialized_) {
        const uint8_t reason = mqtt_->lastDisconnectReason();
        const bool authReject = (reason == 4 || reason == 5);
        authRejectStreak_ = authReject ? (uint8_t)(authRejectStreak_ + 1) : 0;
        if (authRejectStreak_ >= 3) {
            HAL_LOG_WARN("CLOUD", "MQTT auth rejected x%u — refreshing token via provision", authRejectStreak_);
            authRejectStreak_ = 0;
            mqtt_->clearLastDisconnectReason();
            mqtt_->disconnect();
            mqttInitialized_ = false;
            mqttRetryCurrentMs_ = 0;
            if (refreshToken()) {
                // Новый токен → сначала check-claim, только потом MQTT.
                lastClaimPoll_ = HAL_MILLIS() - config_.claimPollIntervalMs;
                setState(CloudState::AwaitingClaim);
            }
            // refreshToken не удался (cooldown/отказ) — остаёмся, backoff
            // продолжит попытки, кирпича нет.
            return;
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

bool CloudStateMachine::requestClaim() {
    idryer::ClaimRequestResult result = requestClaimDetailed();
    return result == idryer::ClaimRequestResult::Started ||
           result == idryer::ClaimRequestResult::AlreadyClaimed;
}

idryer::ClaimRequestResult CloudStateMachine::requestClaimDetailed() {
    if (identity_.hasDeviceId()) {
        HAL_LOG_WARN("CLOUD", "Local NVS has deviceId=%s, checking backend claim state...", identity_.deviceId);
        emitDiagnostic2("claim: local NVS has deviceId=", identity_.deviceId);
        emitDiagnostic("claim: checking backend");
        if (!identity_.hasToken()) {
            HAL_LOG_WARN("CLOUD", "Stale claim NVS: deviceId exists but token is missing");
            emitDiagnostic("claim: stale NVS, token is missing");
            return idryer::ClaimRequestResult::StaleNvs;
        }

        ClaimCheckResult claim = api_->checkClaim(identity_.token);
        if (claim.success && claim.claimed) {
            if (claim.deviceId[0] != '\0' && strcmp(claim.deviceId, identity_.deviceId) != 0) {
                identity_.setDeviceId(claim.deviceId);
                store_->save(identity_);
            }
            HAL_LOG_INFO("CLOUD", "Backend confirms claim: deviceId=%s", identity_.deviceId);
            emitDiagnostic2("claim: backend confirmed deviceId=", identity_.deviceId);
            return idryer::ClaimRequestResult::AlreadyClaimed;
        }

        HAL_LOG_WARN("CLOUD", "Stale claim NVS: backend did not confirm deviceId=%s", identity_.deviceId);
        emitDiagnostic2("claim: stale NVS, backend did not confirm deviceId=", identity_.deviceId);
        return idryer::ClaimRequestResult::StaleNvs;
    }

    if (!wifi_->isConnected()) { HAL_LOG_ERROR("CLOUD", "WiFi not connected"); return idryer::ClaimRequestResult::WifiNotConnected; }
    if (config_.waitForMcuSerial && !serialVerified_) {
        HAL_LOG_WARN("CLOUD", "Claim rejected: waiting for MCU serial");
        return idryer::ClaimRequestResult::WaitingForMcuSerial;
    }

    if (!identity_.hasToken()) {
        HAL_LOG_INFO("CLOUD", "No token, doing provision first...");
        ProvisionResult provResult = api_->provision(identity_.serialNumber);
        if (!provResult.success) { HAL_LOG_ERROR("CLOUD", "Provision failed"); return idryer::ClaimRequestResult::ProvisionFailed; }
        if (provResult.isClaimed && provResult.token[0] == '\0') {
            HAL_LOG_WARN("CLOUD", "Serial claimed but token withheld. Delete device in app first.");
            return idryer::ClaimRequestResult::TokenWithheld;
        }
        // binding-v2: пустой токен не пишем — не затираем рабочий секрет.
        if (provResult.token[0] == '\0') {
            HAL_LOG_WARN("CLOUD", "Provision returned no token");
            return idryer::ClaimRequestResult::ProvisionFailed;
        }
        identity_.setToken(provResult.token);
        store_->save(identity_);
        if (provResult.isClaimed && provResult.deviceId[0] != '\0') {
            identity_.setDeviceId(provResult.deviceId);
            store_->save(identity_);
            HAL_LOG_INFO("CLOUD", "Already claimed: %s", identity_.deviceId);
            return idryer::ClaimRequestResult::AlreadyClaimed;
        }
    }

    if (awaitingClaim_) {
        const uint32_t elapsedSec = (HAL_MILLIS() - pinCreatedAtMs_) / 1000;
        const bool     pinAlive   = elapsedSec < pinTotalSeconds_;
        // Пока код жив — повторяем его же с актуальным остатком. Когда истёк,
        // проваливаемся ниже на register: бэкенд отдаёт тот же PIN, пока он
        // действителен, и выпускает новый только после истечения — то есть
        // повторный запрос безопасен, а без него на экране устройства висит
        // мёртвый код до перезагрузки.
        if (pinAlive) {
            HAL_LOG_INFO("CLOUD", "Claim already in progress, PIN=%s", pendingPin_);
            if (claimPinCallback_ && pendingPin_[0] != '\0') {
                claimPinCallback_(pendingPin_, pinTotalSeconds_ - elapsedSec, claimPinCtx_);
            }
            return idryer::ClaimRequestResult::Started;
        }
        HAL_LOG_INFO("CLOUD", "PIN expired (%us), requesting a new one", (unsigned)elapsedSec);
        awaitingClaim_ = false;
    }

    HAL_LOG_INFO("CLOUD", "Registering device for claim...");
    emitDiagnostic("claim: requesting PIN from backend");
    RegisterResult regResult = api_->registerDevice(identity_.token, identity_.serialNumber);
    if (!regResult.success) { HAL_LOG_ERROR("CLOUD", "Register failed"); return idryer::ClaimRequestResult::RegisterFailed; }

    if (regResult.alreadyClaimed && regResult.deviceId[0] != '\0') {
        HAL_LOG_INFO("CLOUD", "Recovery: device already claimed, deviceId=%s", regResult.deviceId);
        identity_.setDeviceId(regResult.deviceId);
        store_->save(identity_);
        setState(CloudState::Ready);
        return idryer::ClaimRequestResult::AlreadyClaimed;
    }

    strncpy(pendingPin_, regResult.pin, sizeof(pendingPin_) - 1);
    pendingPin_[sizeof(pendingPin_)-1] = '\0';
    pinCreatedAtMs_  = HAL_MILLIS();
    pinTotalSeconds_ = regResult.remainingSeconds;
    awaitingClaim_ = true;
    lastClaimPoll_ = HAL_MILLIS() - config_.claimPollIntervalMs;

    HAL_LOG_INFO("CLOUD", "PIN: %s (expires in %us)", pendingPin_, regResult.remainingSeconds);
    emitDiagnostic2("claim: PIN received pin=", pendingPin_);
    if (claimPinCallback_) claimPinCallback_(pendingPin_, regResult.remainingSeconds, claimPinCtx_);

    setState(CloudState::AwaitingClaim);
    return idryer::ClaimRequestResult::Started;
}

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

bool CloudStateMachine::refreshToken() {
    if (!wifi_->isConnected()) { HAL_LOG_WARN("CLOUD", "refreshToken: no WiFi"); return false; }
    if (!identity_.hasSerialNumber()) { HAL_LOG_WARN("CLOUD", "refreshToken: no serial"); return false; }

    const uint32_t now = HAL_MILLIS();
    if (lastTokenRefreshMs_ != 0 && now - lastTokenRefreshMs_ < 30000u) {
        HAL_LOG_INFO("CLOUD", "refreshToken: cooldown active");
        return false;
    }
    lastTokenRefreshMs_ = now;

    ProvisionResult result = api_->provision(identity_.serialNumber);
    if (!result.success || result.token[0] == '\0') { HAL_LOG_WARN("CLOUD", "refreshToken: provision failed"); return false; }

    identity_.setToken(result.token);
    store_->save(identity_);
    HAL_LOG_INFO("CLOUD", "refreshToken: token updated");
    return true;
}

void CloudStateMachine::setState(CloudState newState) {
    if (state_ == newState) return;
    CloudState oldState = state_;
    state_ = newState;
    HAL_LOG_INFO("CLOUD", "State: %s -> %s", cloudStateToString(oldState), cloudStateToString(newState));
    if (stateCallback_) stateCallback_(oldState, newState, stateCallbackCtx_);
}

void CloudStateMachine::setStateChangeCallback(CloudStateChangeCallback cb, void* ctx) {
    stateCallback_ = cb; stateCallbackCtx_ = ctx;
}
void CloudStateMachine::setClaimPinCallback(ClaimPinCallback cb, void* ctx) {
    claimPinCallback_ = cb; claimPinCtx_ = ctx;
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

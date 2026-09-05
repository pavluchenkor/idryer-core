#if defined(ESP32) || defined(ESP_PLATFORM)

#include "EspTouchProvisioner.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

// Логи провижининга идут в USB Serial напрямую: HAL до подъёма Wi-Fi молчит
// (initArduinoHal(nullptr) в Link::begin), а именно эта фаза и нуждается в
// диагностике. На продуктах без экрана тот же Serial держит Improv-RPC
// веб-установщика, поэтому по умолчанию тихо — включается флагом сборки.
#if defined(IDRYER_ESPTOUCH_LOG) || defined(IDRYER_DEV_REPL)
#define ESPTOUCH_LOG(...)  do { Serial.printf(__VA_ARGS__); Serial.flush(); } while (0)
#else
#define ESPTOUCH_LOG(...)  do { } while (0)
#endif

namespace idryer {

EspTouchProvisioner& EspTouchProvisioner::instance() {
    static EspTouchProvisioner s_instance;
    return s_instance;
}

void EspTouchProvisioner::begin(SaveCallback save, void* saveCtx,
                                bool credentialsPresent) {
    if (disabled_) return;
    saveCb_             = save;
    saveCtx_            = saveCtx;
    credentialsPresent_ = credentialsPresent;

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        auto& self = EspTouchProvisioner::instance();
        self.lastEventMs_ = millis();
        self.lastReason_  = info.wifi_sta_disconnected.reason;
        if (self.failStreak_ < 255) self.failStreak_++;
        // Каноника примера ESP-IDF (smart_config): переподключение сразу после
        // разрыва, безусловно. Arduino сам ретраит только часть reason'ов
        // (202/205 не входят), а ожидание «тишины стека» в цикле растягивало
        // подключение на минуты. Событие разрыва == рукопожатие уже
        // завершилось, обрывать нечего — реконнект безопасен.
        // Подключаться прямо отсюда нельзя: esp_wifi_connect(), вызванный из
        // контекста обработчика события после reason 202/39, не запускает
        // новую попытку — стек уходит в тишину на десятки секунд (видно в
        // логах стенда). Поэтому только помечаем «нужен коннект», а сам вызов
        // делает основной цикл.
        self.needConnect_ = true;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
        auto& self = EspTouchProvisioner::instance();
        self.failStreak_    = 0;
        self.airScanned_    = false;
        self.everConnected_ = true;
    }, ARDUINO_EVENT_WIFI_STA_CONNECTED);

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        EspTouchProvisioner::instance().handleGotCredentials(info);
    }, ARDUINO_EVENT_SC_GOT_SSID_PSWD);

    ESPTOUCH_LOG("[WIFI] boot: stored credentials=%s\n",
                 credentialsPresent_ ? "yes" : "no");

    if (!credentialsPresent_) start();
}

void EspTouchProvisioner::start() {
    if (active_ || WiFi.status() == WL_CONNECTED) return;
    // ESPTouch v2: данные передаются полезной нагрузкой multicast-пакетов, а не
    // длинами кадров, как в v1. Длины ломаются, когда роутер ретранслирует
    // broadcast между полосами (одна mesh-сеть с общим SSID на 2.4 и 5 ГГц) —
    // именно поэтому v1 срабатывал через раз. Ключ шифрования не задаём:
    // тогда и приложение работает без него.
    // ВАЖНО: протоколы несовместимы — приложение обязано слать тоже v2.
    if (!WiFi.beginSmartConfig(SC_TYPE_ESPTOUCH_V2)) {
        ESPTOUCH_LOG("[WIFI] ESPTouch v2 start FAILED\n");
        return;
    }
    ESPTOUCH_LOG("[WIFI] ESPTouch v2 listening\n");
    active_            = true;
    gotCredentials_    = false;
    fallbackStartedMs_ = millis();

    if (noticeCb_) {
        // «Проверьте пароль» показываем только там, где это единственное
        // оставшееся объяснение: сеть уже была задана и ни разу не пустила.
        const Notice notice = (everConnected_ || !credentialsPresent_)
                                  ? Notice::Listening
                                  : Notice::CheckPassword;
        noticeCb_(noticeCtx_, notice);
    }
}

void EspTouchProvisioner::stop() {
    if (!active_) return;
    WiFi.stopSmartConfig();
    active_ = false;
    ESPTOUCH_LOG("[WIFI] ESPTouch stopped, radio released\n");
}

void EspTouchProvisioner::handleGotCredentials(const WiFiEventInfo_t& info) {
    // Только принимаем данные: подключаться, пока активен SmartConfig,
    // бесполезно — радио прыгает по каналам, и association уходит в
    // AUTH_EXPIRE. Останавливает режим и подключается loop().
    memcpy(ssid_, info.sc_got_ssid_pswd.ssid, 32);
    memcpy(pass_, info.sc_got_ssid_pswd.password, 64);
    ssid_[32] = '\0';
    pass_[64] = '\0';
    // После этого события стек гасит сниффер: эфир больше не слушается,
    // режим ждёт подключения ради отправки подтверждения. Пишем об этом
    // явно, иначе строка «listening» выше вводит в заблуждение.
    ESPTOUCH_LOG("[WIFI] ESPTouch got ssid=\"%s\" pwdLen=%u — "
                 "listening stopped, connecting\n",
                 ssid_, (unsigned)strlen(pass_));

    // Ровно одно действие подключения, как в примере ESP-IDF
    // (examples/wifi/smart_config): disconnect → set_config → connect.
    // BSSID не задаём: в примере он за CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
    // и по умолчанию выключен, а Arduino подставляет его всегда.
    // SmartConfig НЕ останавливаем — стек сам гасит сниффер
    // (ic_disable_sniffer в логах примера) и после подключения шлёт
    // подтверждение с IP в приложение, по SC_SEND_ACK_DONE.
    wifi_config_t cfg = {};
    memcpy(cfg.sta.ssid, info.sc_got_ssid_pswd.ssid, sizeof(cfg.sta.ssid));
    memcpy(cfg.sta.password, info.sc_got_ssid_pswd.password,
           sizeof(cfg.sta.password));
    cfg.sta.bssid_set = false;
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
    lastConnectMs_  = millis();
    gotCredentials_ = true;
}

bool EspTouchProvisioner::airScan() {
    airScanned_ = true;
    wifi_config_t cur = {};
    esp_wifi_get_config(WIFI_IF_STA, &cur);
    const int found = WiFi.scanNetworks(false, true);
    ESPTOUCH_LOG("[WIFI] air scan: %d networks, last reason=%u\n",
                 found, (unsigned)lastReason_);
    for (int i = 0; i < found; i++) {
        ESPTOUCH_LOG("[WIFI]   \"%s\" rssi=%d ch=%d bssid=%s\n",
                     WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                     WiFi.channel(i), WiFi.BSSIDstr(i).c_str());
        if (WiFi.SSID(i) == (const char*)cur.sta.ssid &&
            (scanBestRssi_ == 127 || WiFi.RSSI(i) > scanBestRssi_)) {
            scanBestRssi_ = (int8_t)WiFi.RSSI(i);
        }
    }
    WiFi.scanDelete();
    return scanBestRssi_ != 127 && scanBestRssi_ <= kWeakRssi;
}

void EspTouchProvisioner::loop() {
    if (disabled_) return;
    // Источников кредов два — веб-установщик (Improv по USB) и ESPTouch.
    // Подключение в обоих случаях запускает стек; здесь только сохраняем в
    // NVS, чтобы они пережили перезагрузку.
    if (gotCredentials_ && ssid_[0]) {
        if (saveCb_) saveCb_(saveCtx_, ssid_, pass_);
        ESPTOUCH_LOG("[WIFI] saved ssid=\"%s\"\n", ssid_);
        provisioned_        = true;
        provisionedAtMs_    = millis();
        credentialsPresent_ = true;
        gotCredentials_     = false;
        ssid_[0]            = '\0';
        airScanned_         = false;
        scanBestRssi_       = 127;
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (active_) {
            active_ = false;
            if (connectedCb_) connectedCb_(connectedCtx_);
        }
        return;
    }

    if (provisioned_) {
        driveConnect();
        return;
    }

    if (!active_) {
        // Сохранённая сеть получает фору: режим настройки поднимаем только
        // если за это время подключиться не вышло.
        if (!credentialsPresent_ || millis() >= kConnectFallbackMs) start();
        return;
    }
    if (millis() - fallbackStartedMs_ >= kRestartMs) {
        WiFi.stopSmartConfig();
        active_ = false;
        start();
    }
}

void EspTouchProvisioner::driveConnect() {
    // Креды уже даны — в режим настройки не возвращаемся, а упорно
    // подключаемся: каждый разрыв даёт новый заход (флаг needConnect_), плюс
    // сторож на случай, когда стек замолчал совсем. Полагаться на авто-повтор
    // Arduino нельзя: reason 202/205 в его список «переподключаемых» не входят.

    // Стек молчит дольше 5 с — значит текущей попытки нет и можно занять
    // радио сканированием, не порвав чужое рукопожатие.
    const bool stackQuiet = millis() - lastEventMs_ >= 5000;

    // После череды неудач один раз смотрим эфир: какие точки нашей сети
    // устройство вообще слышит и с каким уровнем. Без этого причину
    // (слабый сигнал или неверный пароль) не различить.
    if (!airScanned_ && failStreak_ >= 5 && stackQuiet) {
        airScan();
        return;
    }

    // Пароль, судя по всему, неверный: рукопожатие не проходит, а связи с
    // этой сетью не было ни разу. Возвращаемся в режим настройки — иначе
    // из опечатки нет выхода без перепрошивки. Новые креды перезапишут
    // сохранённые.
    const bool authRejected = lastReason_ == 15 ||    // 4WAY_HANDSHAKE_TIMEOUT
                              lastReason_ == 202;     // AUTH_FAIL
    // При слабом сигнале (−80 и хуже) те же reason 15/202 даёт потеря
    // auth-кадров при верном пароле, а подключение может занять минуты —
    // там сброс только по таймеру kWeakSignalGiveUpMs, не по счётчику.
    const bool weakSignal = scanBestRssi_ != 127 && scanBestRssi_ <= kWeakRssi;
    const bool weakGaveUp = millis() - provisionedAtMs_ >= kWeakSignalGiveUpMs;
    if (!everConnected_ && authRejected && failStreak_ >= kAuthFailLimit &&
        (!weakSignal || weakGaveUp)) {
        // Во время шторма переподключений stackQuiet не наступает и скан
        // выше не успевает пройти — RSSI неизвестен. Прежде чем стирать
        // креды, выясняем условия приёма принудительно: останавливаем
        // попытки и сканируем здесь. При слабом сигнале сброс отменяется.
        if (!airScanned_) {
            WiFi.disconnect(false, false);
            if (airScan()) {
                ESPTOUCH_LOG("[WIFI] weak signal (best rssi=%d), "
                             "keep trying instead of reset\n",
                             (int)scanBestRssi_);
                failStreak_    = 0;
                lastConnectMs_ = millis();
                esp_wifi_connect();
                return;
            }
        }
        ESPTOUCH_LOG("[WIFI] credentials rejected (reason=%u), "
                     "back to setup mode\n", (unsigned)lastReason_);
        provisioned_  = false;
        failStreak_   = 0;
        airScanned_   = false;
        scanBestRssi_ = 127;
        WiFi.disconnect(false, false);
        // Обязательно останавливаем режим перед перезапуском. После приёма
        // кредов стек гасит сниффер и ждёт подключения ради подтверждения,
        // то есть эфир уже не слушает, а флаг активности остаётся поднятым
        // — без явной остановки start() просто выходит по нему, и устройство
        // молчит, хотя в логе значится «listening».
        // Повторный esp_smartconfig_start без stop запрещён и самим SDK.
        WiFi.stopSmartConfig();
        active_ = false;
        start();
        return;
    }

    // Новый заход после разрыва: событие лишь пометило флаг, вызов делаем
    // здесь, из основного контекста и с паузой — так стек его принимает.
    if (needConnect_ && millis() - lastEventMs_ >= kReconnectDelayMs) {
        needConnect_   = false;
        lastConnectMs_ = millis();
        const esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESPTOUCH_LOG("[WIFI] connect -> %s (reason=%u)\n",
                         esp_err_to_name(err), (unsigned)lastReason_);
            needConnect_ = true;   // повторим на следующем проходе
        }
        return;
    }

    // Сторож на полную тишину: ни событий, ни подключения. Такое бывает,
    // когда заход не состоялся и стек остался «в процессе». Дёргаем только
    // disconnect — ответное событие поставит флаг, и коннект уйдёт выше по
    // общему пути (disconnect+connect подряд дают ESP_ERR_WIFI_CONN).
    if (millis() - lastEventMs_ >= kWatchdogMs &&
        millis() - lastConnectMs_ >= kWatchdogMs) {
        lastConnectMs_ = millis();
        ESPTOUCH_LOG("[WIFI] watchdog: no events %us, kick stack\n",
                     (unsigned)(kWatchdogMs / 1000));
        esp_wifi_disconnect();
    }
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM

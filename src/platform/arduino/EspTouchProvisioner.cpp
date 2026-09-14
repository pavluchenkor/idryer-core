#if defined(ESP32) || defined(ESP_PLATFORM)

#include "EspTouchProvisioner.h"
#include "../../radio_busy.h"
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

        // Отказ выбранной точки считаем ЗДЕСЬ, по событию разрыва, а не по
        // времени в bootRetry(). Раньше счётчик рос просто раз в kBootRetryMs,
        // то есть считал не отказы, а секунды: подключение длиннее 18 с
        // выглядело как три отказа подряд. Измерено 14.09.2026 — хорошая точка
        // на -62 dBm ушла в бан на 12-й секунде, и следующие пять минут
        // устройство металось по точкам на -86, потому что лучшая была
        // исключена им же самим.
        //
        // Reason 8 (ASSOC_LEAVE) - это МЫ сами разорвали связь: ради переезда
        // или перед сканом в pinBestAp(). Отказом точки это не является.
        const bool selfInitiated = info.wifi_sta_disconnected.reason == 8;
        if (self.apPinned_ && !selfInitiated && self.apPinFails_ < 255)
            self.apPinFails_++;

        // Связи нет — выбор точки устарел. Снимаем отметку, чтобы следующий
        // заход начался со свежего скана: за время работы расстановка сил в
        // эфире могла измениться, а держаться за прежнюю точку незачем.
        self.apPinned_ = false;
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
        // Подключение удалось — счётчик неудачных заходов к выбранной точке
        // обнуляем, иначе привязка отключилась бы навсегда после первой же
        // серии помех.
        self.apPinFails_    = 0;
        // Подключились — взводим наблюдение за уровнем: когда он просядет,
        // стек позовёт нас осмотреться.
        self.armRssiWatch(WiFi.RSSI());
        // Новая точка — прежние наблюдения не значат ничего. Среднее собираем
        // заново, отметка «уровень на прошлом осмотре» сбрасывается, и первый
        // осмотр этой сессии пойдёт через kFirstScanDelayMs.
        self.connectedAtMs_     = millis();
        self.rssiAvg_           = 127;
        self.roamRefRssi_       = 127;
        self.fruitlessScans_    = 0;
        self.lastRssiSampleMs_  = millis();
    }, ARDUINO_EVENT_WIFI_STA_CONNECTED);

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        EspTouchProvisioner::instance().handleGotCredentials(info);
    }, ARDUINO_EVENT_SC_GOT_SSID_PSWD);

    // Подтверждение ушло в телефон — режим отработал своё, гасим. Это штатное
    // место остановки по документации Espressif: до этого события останавливать
    // нельзя, иначе приложение покажет «не удалось» при уже подключившемся
    // устройстве.
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
        auto& self = EspTouchProvisioner::instance();
        self.awaitingScAck_ = false;
        self.stop();
    }, ARDUINO_EVENT_SC_SEND_ACK_DONE);

    // «Сигнал текущей точки просел ниже порога». У Arduino обёртки для этого
    // события нет — подписываемся прямо у стека. Сам обработчик только ставит
    // флаг: скан и решение делает основной цикл.
    esp_event_handler_register(
        WIFI_EVENT, WIFI_EVENT_STA_BSS_RSSI_LOW,
        [](void*, esp_event_base_t, int32_t, void*) {
            EspTouchProvisioner::instance().roamCheckPending_ = true;
        },
        nullptr);

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
        // Отказ надо запомнить. Без этого цикл видит «режим не поднят», зовёт
        // start() снова — и так на каждом обороте, тысячи раз в секунду.
        // Измерено 14.09.2026 на стенде: 60 435 одинаковых строк в логе за
        // десять минут, 3 МБ, за которыми не видно ничего другого.
        startFailedAtMs_ = millis();
        // И попробовать вылечиться: самая частая причина отказа — режим уже
        // поднят в SDK (повторный старт без остановки запрещён). Гасим, чтобы
        // следующая попытка прошла.
        WiFi.stopSmartConfig();
        return;
    }
    startFailedAtMs_ = 0;
    ESPTOUCH_LOG("[WIFI] ESPTouch v2 listening\n");
    active_            = true;
    gotCredentials_    = false;
    fallbackStartedMs_ = millis();

    // «Проверьте пароль» показываем только там, где это единственное
    // оставшееся объяснение: сеть уже была задана и ни разу не пустила.
    // Запоминаем выбор до проверки колбэка: продукт мог ещё не
    // зарегистрироваться, и тогда onNotice() покажет это же при регистрации.
    noticeShown_ = (everConnected_ || !credentialsPresent_)
                       ? Notice::Listening
                       : Notice::CheckPassword;
    if (noticeCb_) {
        noticeCb_(noticeCtx_, noticeShown_);
    }
}

void EspTouchProvisioner::stop() {
    // Намеренно без проверки active_: этот флаг означает «показываем экран
    // настройки», а SmartConfig в SDK живёт своей жизнью и мог остаться
    // поднятым при снятом флаге — именно так и ломалось. Свой флаг у SDK есть,
    // и WiFi.stopSmartConfig() на непущенном режиме просто ничего не делает,
    // так что звать её всегда безопасно.
    const bool wasActive = active_;
    active_ = false;
    if (!WiFi.stopSmartConfig()) {
        ESPTOUCH_LOG("[WIFI] ESPTouch stop FAILED\n");
        return;
    }
    // Молчим, когда гасить было нечего: иначе строка шла бы на каждом обороте.
    if (wasActive) ESPTOUCH_LOG("[WIFI] ESPTouch stopped, radio released\n");
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
    // Режим не гасим до подтверждения: стек ещё отправит в телефон IP.
    awaitingScAck_  = true;
}

void EspTouchProvisioner::armRssiWatch(int /*currentRssi*/) {
    // Порог постоянный, а не относительно текущего уровня. Относительный
    // казался защитой от круговых проверок, но давал худшее: устройство,
    // подключившееся на слабой точке, взводило порог ещё ниже — и уже никогда
    // не осматривалось, даже когда хорошая точка возвращалась (измерено:
    // подключение на −87 dBm, порог −92, ни одной проверки за 4 минуты).
    // От частых проверок защищает kRoamCheckMinMs: пока сигнал ниже порога,
    // осмотр просто повторяется раз в пять минут.
    esp_wifi_set_rssi_threshold(kRssiLowDbm);
}

#ifdef IDRYER_WIFI_SCAN_LOG_MS
void EspTouchProvisioner::logAirScan() {
    const bool linked = WiFi.status() == WL_CONNECTED;
    const String want = linked ? WiFi.SSID() : String();
    uint8_t curBssid[6] = {};
    if (linked) if (const uint8_t* b = WiFi.BSSID()) memcpy(curBssid, b, 6);

    const int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    ESPTOUCH_LOG("[AIR] %lus: %d networks; linked=%s rssi=%d\n",
                 (unsigned long)(millis() / 1000), n,
                 linked ? WiFi.BSSIDstr().c_str() : "no", linked ? WiFi.RSSI() : 0);
    for (int i = 0; i < n; ++i) {
        // Звёздочка — точка, на которой сидим сейчас: по ней сразу видно,
        // лучшая она из слышимых или нет.
        const bool cur = linked && memcmp(WiFi.BSSID(i), curBssid, 6) == 0;
        ESPTOUCH_LOG("[AIR]   %s rssi=%4d ch=%2d %s \"%s\"\n",
                     cur ? "*" : " ", WiFi.RSSI(i), WiFi.channel(i),
                     WiFi.BSSIDstr(i).c_str(), WiFi.SSID(i).c_str());
    }
    WiFi.scanDelete();
}
#endif

void EspTouchProvisioner::sampleRssi() {
    if (millis() - lastRssiSampleMs_ < kRssiSampleMs) return;
    lastRssiSampleMs_ = millis();
    const int8_t now = (int8_t)WiFi.RSSI();
    // Первый замер после подключения задаёт среднее целиком: стартовать с нуля
    // нельзя, иначе первые полминуты среднее ползёт от 0 к реальным −65 и
    // выглядит как чудовищная просадка.
    rssiAvg_ = (rssiAvg_ == 127) ? now : (int8_t)((rssiAvg_ + now) / 2);
}

void EspTouchProvisioner::roamScanStart(int minGainDb) {
    // Скан асинхронный. Блокирующий WiFi.scanNetworks() останавливал весь
    // главный цикл на ~3,2 с (измерено 14.09.2026 по отметкам времени хоста) —
    // вместе с обменом замирали экран и тач, что на сенсорном устройстве
    // заметнее любой задержки телеметрии.
    //
    // Через Arduino, а не esp_wifi_scan_start(): библиотека держит собственный
    // обработчик WIFI_EVENT_SCAN_DONE и сама забирает результаты. Свой скан
    // мимо неё гонялся бы с ней за один и тот же список.
    //
    // Скрытые сети не нужны (у них пустое имя, в нашу сеть они не попадают), а
    // имя сети в запросе делает probe направленным.
    const String want = WiFi.SSID();
    const int16_t started = WiFi.scanNetworks(
        /*async=*/true, /*show_hidden=*/false, /*passive=*/false,
        kScanMsPerChannel, /*channel=*/0, want.c_str());
    if (started == WIFI_SCAN_FAILED) {
        ESPTOUCH_LOG("[WIFI] roam scan: start failed\n");
        return;
    }
    roamScanRunning_   = true;
    roamScanStartedMs_ = millis();
    roamScanGainDb_    = minGainDb;
}

void EspTouchProvisioner::roamScanFinish() {
    if (WiFi.status() != WL_CONNECTED) { WiFi.scanDelete(); return; }

    const int minGainDb = roamScanGainDb_;
    const int curRssi = WiFi.RSSI();
    uint8_t curBssid[6] = {};
    if (const uint8_t* b = WiFi.BSSID()) memcpy(curBssid, b, 6);
    const String want = WiFi.SSID();

    const int n = WiFi.scanComplete();
    int best = -1, bestRssi = -128;
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) != want) continue;
        // Тот же список банов, что и у pinBestAp(). Без этого осмотр раз за
        // разом выбирал бы точку, к которой заход всё равно не пойдёт:
        // решение принималось бы по одним правилам, а исполнялось по другим.
        if (isBanned(WiFi.BSSID(i))) continue;
        if (WiFi.RSSI(i) > bestRssi) { bestRssi = WiFi.RSSI(i); best = i; }
    }
    // Остаёмся — запоминаем, от какого уровня считать следующую просадку.
    // Пока сигнал держится около этой отметки, сканов больше не будет вовсе.
    if (best < 0) {
        WiFi.scanDelete();
        roamRefRssi_ = (rssiAvg_ != 127) ? rssiAvg_ : (int8_t)curRssi;
        if (fruitlessScans_ < 255) fruitlessScans_++;
        armRssiWatch(curRssi);
        return;
    }

    const bool sameAp = memcmp(WiFi.BSSID(best), curBssid, 6) == 0;
    const bool worth  = !sameAp && (bestRssi - curRssi >= minGainDb);
    ESPTOUCH_LOG("[WIFI] roam check: current %d dBm (avg %d), best %s %d dBm "
                 "(need +%d) -> %s\n",
                 curRssi, (int)rssiAvg_, WiFi.BSSIDstr(best).c_str(), bestRssi,
                 minGainDb, worth ? "switching" : "staying");

    if (!worth) {
        WiFi.scanDelete();
        roamRefRssi_ = (rssiAvg_ != 127) ? rssiAvg_ : (int8_t)curRssi;
        // Осмотр прошёл впустую — бюджет частых осмотров тратится. Когда он
        // кончится, плановые осмотры уйдут на редкий интервал.
        if (fruitlessScans_ < 255) fruitlessScans_++;
        armRssiWatch(curRssi);
        return;
    }

    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
        memcpy(cfg.sta.bssid, WiFi.BSSID(best), 6);
        cfg.sta.bssid_set = true;
        cfg.sta.channel   = (uint8_t)WiFi.channel(best);
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        apPinned_ = true;
    }
    // Переезд — обстановка явно меняется, бюджет частых осмотров пополняется.
    fruitlessScans_ = 0;
    WiFi.scanDelete();
    // Переход = разрыв и новый заход: событие разрыва поставит needConnect_,
    // и подключение уйдёт общим путём, уже к выбранной точке.
    esp_wifi_disconnect();
}

bool EspTouchProvisioner::pinBestAp() {
    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) return false;
    if (cfg.sta.ssid[0] == '\0') return false;   // имя сети ещё не известно
    const String want((const char*)cfg.sta.ssid);

    // Остановить попытку, которая уже идёт. Без этого вся работа впустую:
    // запущенное подключение конфигурацию больше не перечитывает, и выбранная
    // точка применится в лучшем случае к следующему заходу (измерено: привязка
    // к −62 dBm, подключение к −82). Заодно скан не будет соперничать с
    // подключением за радио — иначе он возвращает ошибку вместо списка.
    esp_wifi_disconnect();
    delay(50);

    // Скан синхронный и со скрытыми сетями: делается только когда связи нет,
    // то есть рвать нечего.
    const int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    // Скан не состоялся — это не «сети не видно». Отрицательное значение
    // возвращается, когда радио занято: например, поднят SmartConfig и его
    // сниффер гоняет каналы. Раньше эти два случая не различались, и неудачный
    // скан молча ронял выбор точки на стек — тот садился куда попало
    // (измерено 14.09.2026: −88 dBm при доступных −66). Привязку в этом случае
    // не трогаем: прошлый выбор остаётся в силе, а заход повторится.
    if (n < 0) {
        ESPTOUCH_LOG("[WIFI] pick AP: scan unavailable (%d), keeping previous choice\n", n);
        return false;
    }
    int best = -1;
    int bestRssi = -128;
    int seenOurs = 0;          // сколько точек нашей сети вообще слышно
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) != want) continue;
        ++seenOurs;
        // Точка в бане — к ней уже не достучались, пропускаем.
        if (isBanned(WiFi.BSSID(i))) continue;
        if (WiFi.RSSI(i) > bestRssi) { bestRssi = WiFi.RSSI(i); best = i; }
    }
    // Все слышимые точки сети оказались в бане. Значит бан больше вредит, чем
    // помогает: снимаем его целиком и выбираем из всех заново. Без этого
    // устройство осталось бы без связи, пока баны не истекут.
    if (best < 0 && seenOurs > 0) {
        ESPTOUCH_LOG("[WIFI] pick AP: all %d APs banned, clearing bans\n", seenOurs);
        memset(bannedAps_, 0, sizeof(bannedAps_));
        for (int i = 0; i < n; ++i) {
            if (WiFi.SSID(i) != want) continue;
            if (WiFi.RSSI(i) > bestRssi) { bestRssi = WiFi.RSSI(i); best = i; }
        }
    }
    if (best < 0) {
        // Нашей сети не слышно вовсе — привязывать не к чему, пусть стек
        // пробует как умеет.
        ESPTOUCH_LOG("[WIFI] pick AP: \"%s\" not visible among %d networks\n",
                     want.c_str(), n);
        WiFi.scanDelete();
        return false;
    }

    memcpy(cfg.sta.bssid, WiFi.BSSID(best), 6);
    cfg.sta.bssid_set = true;
    cfg.sta.channel   = (uint8_t)WiFi.channel(best);
    const esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    ESPTOUCH_LOG("[WIFI] pick AP: %s rssi=%d ch=%d (best of %d in \"%s\") -> %s\n",
                 WiFi.BSSIDstr(best).c_str(), bestRssi, WiFi.channel(best), n,
                 want.c_str(), esp_err_to_name(err));
    WiFi.scanDelete();
    if (err != ESP_OK) return false;
    apPinned_ = true;
    return true;
}

bool EspTouchProvisioner::isBanned(const uint8_t bssid[6]) const {
    const uint32_t now = millis();
    for (const auto& b : bannedAps_) {
        if (b.untilMs == 0) continue;
        // Сравнение через вычитание: переживает переполнение millis().
        if ((int32_t)(now - b.untilMs) >= 0) continue;   // бан истёк
        if (memcmp(b.bssid, bssid, 6) == 0) return true;
    }
    return false;
}

void EspTouchProvisioner::banAp(const uint8_t bssid[6]) {
    const uint32_t now = millis();
    // Свободное место или самая старая запись — список короткий, перебором.
    BannedAp* slot = nullptr;
    for (auto& b : bannedAps_) {
        if (memcmp(b.bssid, bssid, 6) == 0) { slot = &b; break; }   // уже есть
        if (b.untilMs == 0 || (int32_t)(now - b.untilMs) >= 0) { slot = &b; break; }
    }
    if (!slot) slot = &bannedAps_[0];
    memcpy(slot->bssid, bssid, 6);
    slot->untilMs = now + kApBanMs;
    ESPTOUCH_LOG("[WIFI] pick AP: %02X:%02X:%02X:%02X:%02X:%02X banned for %us\n",
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                 (unsigned)(kApBanMs / 1000));
}

void EspTouchProvisioner::retireFailedAp() {
    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) return;
    if (cfg.sta.bssid_set) {
        banAp(cfg.sta.bssid);
        cfg.sta.bssid_set = false;
        cfg.sta.channel   = 0;     // 0 — искать по всем каналам
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
    }
    apPinned_   = false;
    apPinFails_ = 0;               // счётчик пойдёт заново, уже на новую точку
    // Целимся в следующую по уровню. Не вышло (сети не слышно или скан не
    // состоялся) — заход уйдёт по имени сети, как было раньше.
    pinBestAp();
}

void EspTouchProvisioner::unpinAp() {
    if (!apPinned_) return;
    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
        cfg.sta.bssid_set = false;
        cfg.sta.channel   = 0;        // 0 — искать по всем каналам
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
    }
    // Счётчик неудач НЕ сбрасываем: иначе следующая же попытка снова выберет
    // точку, снова не достучится, и так по кругу. Он обнуляется только при
    // успешном подключении.
    apPinned_ = false;
    ESPTOUCH_LOG("[WIFI] pick AP: unpinned, connecting by SSID again\n");
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
        offlineSinceMs_ = 0;   // связь есть — отсчёт до режима настройки сброшен
        if (active_) {
            active_ = false;
            if (connectedCb_) connectedCb_(connectedCtx_);
        }
        // Погасить режим настройки совсем, а не только убрать его с экрана.
        //
        // Раньше здесь снимался только флаг active_, а esp_smartconfig_stop()
        // не звался — Arduino сам его тоже не зовёт (WiFiGeneric лишь
        // пробрасывает событие). Режим оставался поднятым в SDK: сниффер гонял
        // радио по каналам при живой связи, скан возвращал −2, выбор точки не
        // работал, а следующая попытка поднять режим упиралась в отказ SDK.
        //
        // Единственное исключение — пароль только что приняли по воздуху.
        // Тогда стек ещё должен отправить в телефон подтверждение с IP
        // (SC_SEND_ACK_DONE): остановка до него оставит приложение с «не
        // удалось» при уже подключившемся устройстве. Ждём подтверждения, но
        // не вечно — kScAckWaitMs, иначе режим завис бы навсегда.
        if (awaitingScAck_ && millis() - provisionedAtMs_ >= kScAckWaitMs) {
            ESPTOUCH_LOG("[WIFI] ESPTouch: no ack from app in %us, stopping anyway\n",
                         (unsigned)(kScAckWaitMs / 1000));
            awaitingScAck_ = false;
        }
        if (!awaitingScAck_) stop();
        // Сигнал просел — осмотреться, не появилось ли точки получше. Место
        // именно здесь: проверка нужна при живой связи, а ниже по функции
        // разбираются случаи, когда связи нет. Решение о переходе принимает
        // roamCheck(), здесь только частота и контекст вызова.
#ifdef IDRYER_WIFI_SCAN_LOG_MS
        // Диагностика стенда: снимок эфира по таймеру. Радио уважаем — во
        // время передачи меню или обновления не лезем.
        if (!radioBusy() && millis() - lastScanLogMs_ >= IDRYER_WIFI_SCAN_LOG_MS) {
            lastScanLogMs_ = millis();
            logAirScan();
        }
#endif
        sampleRssi();

        // Скан уже идёт — ждём его. scanComplete() отдаёт число точек, когда
        // готово, и отрицательное значение, пока нет.
        if (roamScanRunning_) {
            const int16_t done = WiFi.scanComplete();
            if (done >= 0) {
                roamScanRunning_ = false;
                lastRoamCheckMs_ = millis();
                roamScanFinish();
            } else if (millis() - roamScanStartedMs_ >= kScanTimeoutMs) {
                // Сторож: скан не завершился. Освобождаем состояние, иначе
                // осмотров больше не будет никогда.
                ESPTOUCH_LOG("[WIFI] roam scan: timeout, giving up\n");
                roamScanRunning_ = false;
                lastRoamCheckMs_ = millis();
                WiFi.scanDelete();
            }
            return;
        }

        // Поводов осмотреться два, и ни один не периодический: пока связь
        // держится, прошивка не сканирует вообще.
        //
        //   1. Первый осмотр после подключения — чинит неудачный выбор точки
        //      стеком. Планка перехода высокая (kFirstScanGainDb): связь
        //      рабочая, рвать её ради пары децибел невыгодно.
        //   2. Просадка сглаженного уровня на kRoamDropDb от отметки прошлого
        //      осмотра. Связь ухудшилась — планка ниже (kRoamGainDb).
        //
        // Событие стека «сигнал просел ниже порога» оставлено третьим поводом:
        // оно почти всегда мертво (приходит только по пересечению порога), но
        // ничего не стоит и иногда срабатывает раньше повода 2.
        //
        // radioBusy() — обновление прошивки, публикация меню, привязка
        // устройства или рукопожатие с брокером прямо сейчас. Отметку «надо
        // осмотреться» не сбрасываем: осмотримся, когда дело закончится.
        if (!radioBusy()) {
            const uint32_t now     = millis();
            const bool     cooled  = lastRoamCheckMs_ == 0 ||
                                     now - lastRoamCheckMs_ >= kRoamCheckMinMs;
            int            minGain = 0;
            if (roamRefRssi_ == 127) {
                // В этой сессии связи ещё не осматривались.
                if (now - connectedAtMs_ >= kFirstScanDelayMs) minGain = kFirstScanGainDb;
            } else if (roamCheckPending_ && cooled) {
                roamCheckPending_ = false;
                minGain           = kRoamGainDb;
            } else if (rssiAvg_ != 127 && rssiAvg_ <= roamRefRssi_ - kRoamDropDb && cooled) {
                ESPTOUCH_LOG("[WIFI] rssi dropped: avg %d vs %d at last scan\n",
                             (int)rssiAvg_, (int)roamRefRssi_);
                // Обстановка изменилась — бюджет частых осмотров пополняется.
                fruitlessScans_ = 0;
                minGain = kRoamGainDb;
            } else {
                // Плановый осмотр. Интервал зависит от того, сколько осмотров
                // подряд уже прошли впустую: пока бюджет цел — частый режим,
                // дальше редкий. Планка перехода высокая: связь сейчас
                // рабочая, повод спокойный.
                const uint32_t every = (fruitlessScans_ < kFallbackBudget)
                                           ? kFallbackFastMs
                                           : kFallbackSlowMs;
                if (now - lastRoamCheckMs_ >= every) minGain = kFirstScanGainDb;
            }
            if (minGain != 0) roamScanStart(minGain);
        }
        return;
    }

    if (provisioned_) {
        driveConnect();
        return;
    }

    // Связи нет — засекаем, с какого момента. Отсюда отсчитывается фора
    // сохранённой сети.
    if (offlineSinceMs_ == 0) offlineSinceMs_ = millis();

    if (!active_) {
        // Сохранённая сеть получает фору: режим настройки поднимаем только
        // если за это время подключиться не вышло. Фору тратим на повторные
        // заходы — просто ждать бессмысленно, стучаться в сеть больше некому.
        //
        // Фора считается от ПОТЕРИ СВЯЗИ, а не от аптайма. Раньше сравнивалось
        // millis() >= kConnectFallbackMs, то есть после полутора минут работы
        // любой обрыв мгновенно поднимал режим настройки: перезагрузился
        // роутер — и устройство с исправным паролем показывает человеку экран
        // перенастройки, а сниффер мешает ему же вернуться в сеть. Измерено
        // 14.09.2026 на стенде при выключении точки.
        if (credentialsPresent_) bootRetry();
        const bool graceOver = millis() - offlineSinceMs_ >= kConnectFallbackMs;
        const bool canStart  = startFailedAtMs_ == 0 ||
                               millis() - startFailedAtMs_ >= kStartRetryMs;
        if ((!credentialsPresent_ || graceOver) && canStart) start();
        return;
    }
    if (millis() - fallbackStartedMs_ >= kRestartMs) {
        WiFi.stopSmartConfig();
        active_ = false;
        start();
    }
}

void EspTouchProvisioner::bootRetry() {
    // Только фаза загрузки с паролем из NVS. Диагностика здесь не нужна:
    // пароль не новый, и если за отведённое окно связи нет — зовём человека,
    // а не пытаемся угадать причину. Различение «опечатка / слабый сигнал»
    // живёт в driveConnect(), где пароль только что ввели.
    const uint32_t now = millis();
    if (now - lastBootTryMs_ < kBootRetryMs) return;
    lastBootTryMs_ = now;

    // Выбор точки — здесь же, где и сам заход: пока попыток немного, целимся в
    // самую сильную, а если она так и не отвечает, снимаем привязку и идём по
    // имени сети, как раньше.
    //
    // Повторно не выбираем, пока привязка стоит: заход, начатый прошлой
    // попыткой, может быть ещё в работе, а скан его прерывает — так подключение
    // уезжало с 6-й секунды на 12-ю. Признак «нужен новый выбор» — снятая
    // привязка: её сбрасывает разрыв связи.
    if (apPinFails_ >= kApPinFails) retireFailedAp();
    else if (!apPinned_)            pinBestAp();
    
    // Подключение при сохранённом пароле делает именно этот вызов — на
    // конфигурации, которая уже лежит в стеке. Печатаем её перед попыткой:
    // scan_method=1 — полный обход каналов, 0 — быстрый скан (стек берёт первую
    // подходящую точку, а не лучшую); sort_method=0 — выбор по уровню сигнала;
    // bssid_set=1 — привязка к одной конкретной точке, тогда выбора нет вовсе.
    // Без этих цифр нельзя понять, применилась ли настройка выбора точки.
    {
        wifi_config_t cur = {};
        if (esp_wifi_get_config(WIFI_IF_STA, &cur) == ESP_OK)
            ESPTOUCH_LOG("[WIFI] sta cfg: scan_method=%d sort_method=%d "
                         "bssid_set=%d channel=%d ssid=\"%s\"\n",
                         (int)cur.sta.scan_method, (int)cur.sta.sort_method,
                         (int)cur.sta.bssid_set, (int)cur.sta.channel,
                         (const char*)cur.sta.ssid);
    }

    const esp_err_t err = esp_wifi_connect();
    // Печатаем и удачный вызов: иначе по логу не отличить «повторы идут» от
    // «их нет», а до подключения HAL-логи ещё молчат — видно только это.
    // ESP_ERR_WIFI_CONN здесь норма: попытка уже идёт, ждём её исхода.
    ESPTOUCH_LOG("[WIFI] boot retry #%u at %lus -> %s\n",
                 (unsigned)++bootTries_, (unsigned long)(now / 1000),
                 esp_err_to_name(err));
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
        // Тот же выбор точки, что и на загрузке: разрыв — подходящий момент
        // пересмотреть, какая точка сейчас слышна лучше.
        if (apPinFails_ >= kApPinFails) retireFailedAp();
        else if (!apPinned_)            pinBestAp();
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

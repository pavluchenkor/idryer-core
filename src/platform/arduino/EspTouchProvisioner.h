#pragma once

#include <WiFi.h>
#include <stddef.h>
#include <stdint.h>

namespace idryer {

/**
 * @brief ESPTouch v2 (SmartConfig) Wi-Fi provisioning for ESP32 products.
 *
 * Второй источник кредов рядом с Improv: Improv раздаёт их по USB из
 * веб-установщика, ESPTouch — по воздуху из мобильного приложения. Режим
 * поднимается сам, когда в NVS нет сети, и после таймаута подключения с
 * сохранёнными кредами — то есть везде, где устройство осталось без связи.
 *
 * Класс владеет подключением целиком, пока Wi-Fi не поднялся: приём кредов,
 * повторные заходы после разрыва, различение «неверный пароль» и «слабый
 * сигнал», возврат в режим настройки. С @c CloudStateMachine он не спорит —
 * та начинает работать только после WL_CONNECTED (ранний выход в
 * @c Link::loop()), когда провижининг уже закончен.
 *
 * Singleton: обработчики событий Wi-Fi регистрируются как non-capturing
 * лямбды и добираются до состояния только через @c instance().
 *
 * Продукту вызывать ничего не нужно — @c Link поднимает и крутит его сам.
 * Колбэки нужны лишь для отображения хода настройки на экране (idryer-touch).
 */
class EspTouchProvisioner {
public:
    /// Что показать пользователю, когда устройство ждёт креды по воздуху.
    enum class Notice : uint8_t {
        /// Слушаем эфир: обычный режим настройки.
        Listening,
        /// Уже слушаем повторно — прошлый пароль сеть не приняла.
        CheckPassword,
    };

    using NoticeCallback    = void (*)(void* ctx, Notice notice);
    using ConnectedCallback = void (*)(void* ctx);
    /// Сохранить принятую сеть: NVS + работающий Wi-Fi-менеджер.
    using SaveCallback      = void (*)(void* ctx, const char* ssid, const char* password);

    static EspTouchProvisioner& instance();

    /**
     * @brief Регистрирует обработчики событий Wi-Fi и запускает режим,
     *        если сохранённых кредов нет.
     *
     * Вызывается из @c Link::begin() после восстановления кредов из NVS.
     *
     * @param save              Куда отдать принятую сеть (@c Link::setWifiCredentials).
     * @param saveCtx           Контекст для @p save.
     * @param credentialsPresent В NVS уже есть сеть (тогда сначала пробуем её).
     */
    void begin(SaveCallback save, void* saveCtx, bool credentialsPresent);

    /// @brief Крутится из @c Link::loop() до подъёма Wi-Fi. No-op после begin() без ESP32.
    void loop();

    /**
     * @brief Гасит режим настройки, освобождая радио.
     *
     * Пока SmartConfig активен, сниффер гоняет радио по каналам и чужое
     * подключение уходит в AUTH_EXPIRE. Нужно всем, кто заводит Wi-Fi мимо
     * этого класса — прежде всего Improv из веб-установщика.
     */
    void stop();

    /**
     * @brief Полностью отключает модуль. Вызывать ДО @c Link::begin().
     *
     * Для продуктов со своей реализацией провижининга (idryer-touch до
     * перевода на ядро): иначе на одном устройстве окажутся два обработчика
     * событий Wi-Fi и два хозяина @c esp_wifi_connect().
     */
    void disable() { disabled_ = true; }

    /// @brief Режим настройки активен — эфир слушается прямо сейчас.
    bool isActive() const { return active_; }

    /// @brief Креды получены в этой сессии; в режим настройки уже не вернёмся.
    bool isProvisioned() const { return provisioned_; }

    /// @brief Экран настройки: вызывается при каждом входе в режим.
    void onNotice(NoticeCallback cb, void* ctx) { noticeCb_ = cb; noticeCtx_ = ctx; }

    /// @brief Wi-Fi поднялся — продукт убирает экран настройки.
    void onConnected(ConnectedCallback cb, void* ctx) { connectedCb_ = cb; connectedCtx_ = ctx; }

private:
    EspTouchProvisioner() = default;

    void start();
    void handleGotCredentials(const WiFiEventInfo_t& info);
    void driveConnect();
    /// @brief Скан эфира; @c true — нашу сеть слышно слабее kWeakRssi.
    bool airScan();

    SaveCallback saveCb_  = nullptr;
    void*        saveCtx_ = nullptr;

    bool     disabled_           = false;
    bool     active_             = false;
    bool     credentialsPresent_ = false;
    // Креды получены в этой сессии. После этого ESPTouch не запускается снова:
    // слушать эфир больше незачем, остаётся только подключаться.
    bool     provisioned_        = false;
    uint32_t provisionedAtMs_    = 0;
    uint32_t fallbackStartedMs_  = 0;

    // Заполняются обработчиком SC_GOT_SSID_PSWD, читаются в loop().
    volatile bool gotCredentials_ = false;
    char     ssid_[33] = {};
    char     pass_[65] = {};

    // Время последнего события Wi-Fi. По нему видно, жив ли процесс
    // подключения: пока события идут, стек работает; полная тишина дольше
    // kWatchdogMs означает, что заход не состоялся и стек надо дёрнуть.
    uint32_t lastEventMs_    = 0;
    uint32_t lastConnectMs_  = 0;
    uint8_t  lastReason_     = 0;
    uint8_t  failStreak_     = 0;
    bool     airScanned_     = false;
    // Лучший RSSI нашей сети по результату air scan. 127 — скан ещё не делался
    // или сеть не найдена. Нужен, чтобы отличить опечатку в пароле от слабого
    // сигнала: обе дают одинаковые reason 15/202.
    int8_t   scanBestRssi_   = 127;
    // Ни одного успешного подключения с этими кредами в текущей сессии. Нужно,
    // чтобы отличить «пароль неверный» от «связь моргнула у рабочей сети».
    bool     everConnected_  = false;
    // Разрыв случился — нужен новый заход. Ставится в обработчике события,
    // исполняется в основном цикле: вызов из контекста обработчика стек
    // игнорирует (см. комментарий в обработчике DISCONNECTED).
    volatile bool needConnect_ = false;

    NoticeCallback    noticeCb_     = nullptr;
    void*             noticeCtx_    = nullptr;
    ConnectedCallback connectedCb_  = nullptr;
    void*             connectedCtx_ = nullptr;

    // Столько неудач аутентификации подряд считаем доказательством неверного
    // пароля и возвращаемся в режим настройки. Иначе устройство молча
    // повторяет попытки до перепрошивки — при опечатке выхода не было вовсе.
    static constexpr uint8_t  kAuthFailLimit = 8;
    // На слабом сигнале (≤ −80) reason 15/202 приходит и при верном пароле, а
    // подключение может занять минуты — счётчик неудач там не годится. Сброс в
    // режим настройки происходит только если за это время не было ни одного
    // успешного подключения.
    static constexpr uint32_t kWeakSignalGiveUpMs = 600000;
    // Пауза между событием разрыва и нашим esp_wifi_connect(). Без неё стек
    // ещё не успевает выйти из предыдущего состояния и возвращает
    // ESP_ERR_WIFI_CONN.
    static constexpr uint32_t kReconnectDelayMs = 700;
    // Сторож на случай полной тишины стека: событий разрыва нет, подключения
    // нет. 20 с — заведомо больше нормального цикла попытки (1–15 с), поэтому
    // в живой процесс подключения он не вмешивается.
    static constexpr uint32_t kWatchdogMs = 20000;
    // Столько ждём сохранённую сеть, прежде чем поднять режим настройки.
    static constexpr uint32_t kConnectFallbackMs = 20000;
    // Перезапуск ESPTouch — страховка от залипания, не рабочий механизм. Он
    // сбрасывает накопленный поток, поэтому период должен быть НАМНОГО больше
    // окна отправки приложения (90 с): при 180 с примерно каждый третий прогон
    // рестарт попадал в середину передачи, и устройство «не слышало» приложение.
    static constexpr uint32_t kRestartMs = 600000;
    // Слабее этого уровня reason 15/202 объясняется приёмом, а не паролем.
    static constexpr int8_t   kWeakRssi = -80;
};

} // namespace idryer

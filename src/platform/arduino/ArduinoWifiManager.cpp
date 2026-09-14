#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoWifiManager.h"
#include "../../hal/hal_types.h"
#include <esp_wifi.h>
#include <string.h>

// Настройка радио происходит раньше, чем поднимается Wi-Fi, а HAL до этого
// момента молчит (initArduinoHal(nullptr) в Link::begin) — через HAL_LOG эту
// фазу не увидеть вовсе. Поэтому тот же приём, что в EspTouchProvisioner:
// прямой Serial под флагом сборки. На продуктах без экрана тот же Serial занят
// Improv-RPC веб-установщика, поэтому по умолчанию тихо.
#if defined(IDRYER_ESPTOUCH_LOG) || defined(IDRYER_DEV_REPL)
#define WIFI_EARLY_LOG(...)  do { Serial.printf(__VA_ARGS__); Serial.flush(); } while (0)
#else
#define WIFI_EARLY_LOG(...)  do { } while (0)
#endif

namespace idryer {

ArduinoWifiManager::ArduinoWifiManager() {
  memset(ssid_, 0, sizeof(ssid_));
  memset(password_, 0, sizeof(password_));
}

void ArduinoWifiManager::begin(const char *ssid, const char *password) {
  if (ssid) {
    strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';
  }
  if (password) {
    strncpy(password_, password, sizeof(password_) - 1);
    password_[sizeof(password_) - 1] = '\0';
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
#ifdef IDRYER_WIFI_TX_POWER
  // TX-мощность задаётся из platformio.ini на платах со слабым питанием
  // антенны (напр. ESP32-C3 Super Mini). Без флага — дефолт SDK (макс).
  WiFi.setTxPower(IDRYER_WIFI_TX_POWER);
  const bool txRequested = true;
  const int  txWanted    = (int)(IDRYER_WIFI_TX_POWER);
#else
  const bool txRequested = false;
  const int  txWanted    = 0;
#endif
  WiFi.setAutoReconnect(true);
  esp_wifi_set_ps(WIFI_PS_NONE);

  // Выбор точки доступа. Умолчание стека — «быстрый скан»: чип цепляется к
  // ПЕРВОЙ встреченной точке с нужным именем сети, а не к самой сильной. В доме
  // с несколькими точками на одном SSID (репитер, mesh) выбор получается
  // случайным — какая точка попадётся первой, зависит от того, с какого канала
  // начался обход. Измерено 13.09.2026: одна и та же плата с одними и теми же
  // настройками подключалась то на −63, то на −84 dBm.
  // Полный обход каналов с выбором по уровню сигнала стоит лишней секунды-двух
  // при подключении, зато точка выбирается лучшая из доступных.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  // Тот же выбор — и для пути без WiFi.begin(): при сохранённых credentials
  // подключение делает esp_wifi_connect() из EspTouchProvisioner, а он берёт
  // конфиг из стека, куда настройки Arduino не попадают.
  {
    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
      cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
      cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
      esp_wifi_set_config(WIFI_IF_STA, &cfg);
    }
  }

  // Мощность читаем обратно из стека, а не печатаем константу сборки: SDK
  // вправе не принять заданное значение молча, и тогда весь замер стабильности
  // оказался бы замером совсем другой мощности. Единицы — четверти dBm
  // (34 = 8.5 dBm), как в wifi_power_t.
  const int txActual = (int)WiFi.getTxPower();
  if (txRequested)
    WIFI_EARLY_LOG("[WIFI] tx power: requested %d, actual %d (quarter-dBm; %d.%d dBm)\n",
                   txWanted, txActual, txActual / 4, (txActual % 4) * 25);
  else
    WIFI_EARLY_LOG("[WIFI] tx power: SDK default, actual %d (quarter-dBm; %d.%d dBm)\n",
                   txActual, txActual / 4, (txActual % 4) * 25);

  HAL_LOG_INFO("WIFI", "Initialized with SSID: %s (tx power %d)", ssid_, txActual);

#if defined(IDRYER_ESPTOUCH_LOG) || defined(IDRYER_DEV_REPL)
  // Разовый снимок эфира — только для стенда. Показывает, сколько точек с нашим
  // именем сети видно и насколько они отличаются по уровню: без этого списка
  // невозможно понять, плохо ли ловит плата или она просто выбрала не ту точку.
  // Скан задерживает старт на секунду-две, поэтому в боевую сборку не попадает
  // (флаг ставится только в env стенда) и делается один раз за загрузку.
  static bool s_airLogged = false;
  if (!s_airLogged) {
    s_airLogged = true;
    const int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    // Имя сети на этом шаге может быть ещё не заполнено (begin(nullptr) при
    // сохранённых credentials) — тогда помечать «нашу» точку нечем, и пометки
    // просто нет. Сравнивать с пустой строкой нельзя: под неё попадают все
    // скрытые сети, и метка начинает врать.
    const bool haveSsid = ssid_[0] != '\0';
    WIFI_EARLY_LOG("[WIFI] air: %d networks visible%s%s%s\n", n,
                   haveSsid ? ", looking for \"" : " (our SSID not known yet)",
                   haveSsid ? ssid_ : "", haveSsid ? "\"" : "");
    for (int i = 0; i < n; ++i) {
      const bool ours = haveSsid && (WiFi.SSID(i) == String(ssid_));
      WIFI_EARLY_LOG("[WIFI]   %s rssi=%4d ch=%2d bssid=%s ssid=\"%s\"\n",
                     ours ? "OURS" : "    ", WiFi.RSSI(i), WiFi.channel(i),
                     WiFi.BSSIDstr(i).c_str(), WiFi.SSID(i).c_str());
    }
    WiFi.scanDelete();
  }
#endif
}

bool ArduinoWifiManager::connect() {
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED)
    return true;

  if (!scanLogged_) {
    HAL_LOG_INFO("WIFI", "Scanning networks...");
    const int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; ++i)
      HAL_LOG_DEBUG("WIFI", "AP %d: %s (RSSI=%d dBm)", i + 1,
                    WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    WiFi.scanDelete();
    scanLogged_ = true;
    HAL_LOG_INFO("WIFI", "Connecting to SSID: %s", ssid_);
    startConnect();
  } else {
    HAL_LOG_DEBUG("WIFI", "Status: %d, reconnecting...", status);
    startConnect();
  }
  return false;
}

// Заход в сеть, не ломая выбор точки.
//
// WiFi.begin(ssid, password) собирает конфигурацию с нуля: bssid_set=0,
// channel=0. Привязка к точке, которую выбрал EspTouchProvisioner::pinBestAp(),
// при этом пропадает, и стек садится на произвольную точку сети. Измерено
// 13.09.2026: выбрали точку на −86 dBm, подключились к −95, и сходу уйти с неё
// уже не получалось.
//
// WiFi.begin() без аргументов читает конфигурацию из стека и пишет обратно как
// есть — привязка заход переживает. Той же формой пользуется и авто-повтор
// самого Arduino, поэтому выбор точки не ломает и он.
//
// Имя сети задаём заново только когда в стеке лежит другое: сеть сменили, и
// привязка к точке прошлой сети всё равно бессмысленна.
void ArduinoWifiManager::startConnect() {
  wifi_config_t cur = {};
  const bool sameNetwork =
      ssid_[0] != '\0' && esp_wifi_get_config(WIFI_IF_STA, &cur) == ESP_OK &&
      strncmp((const char *)cur.sta.ssid, ssid_, sizeof(cur.sta.ssid)) == 0;

  if (sameNetwork)
    WiFi.begin();
  else
    WiFi.begin(ssid_, password_);
}

bool ArduinoWifiManager::isConnected() { return WiFi.status() == WL_CONNECTED; }

void ArduinoWifiManager::disconnect() {
  WiFi.disconnect();
  scanLogged_ = false;
}

void ArduinoWifiManager::getLocalIP(char *buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0)
    return;
  String ip = WiFi.localIP().toString();
  strncpy(buffer, ip.c_str(), bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

void ArduinoWifiManager::getSSID(char *buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0)
    return;
  if (isConnected()) {
    String ssid = WiFi.SSID();
    strncpy(buffer, ssid.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
  } else {
    buffer[0] = '\0';
  }
}

int ArduinoWifiManager::getRSSI() { return WiFi.RSSI(); }

void ArduinoWifiManager::getMacAddress(char *buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0)
    return;
  String mac = WiFi.macAddress();
  strncpy(buffer, mac.c_str(), bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

void ArduinoWifiManager::loop() {}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM

# Kořen kompozice

Produkt vytváří všechny objekty knihovny v `main.cpp` jako statické proměnné a předává závislosti skrze konstruktory. Bez továren, bez globálního registru — jen explicitní montáž.

## Pořadí vytvoření objektů

Závislosti se budují zdola nahoru: nejdříve vrstva platformy, pak cloud stack, pak runtime.

\`\`\`cpp
// 1. Vrstva platformy
idryer::ArduinoWifiStore       s_wifiStore;      // NVS: SSID/password
idryer::ArduinoWifiManager     s_wifi;           // Správa WiFi
idryer::ArduinoCredentialStore s_credentials;    // NVS: serial/token/deviceId
idryer::ArduinoHttpClient      s_http;           // TLS HTTP pro provisioning

// 2. Cloud stack
idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
idryer::MqttClient               s_mqtt;
idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
idryer::ActionDispatcher         s_dispatcher;

// 3. Profil produktu (implementuje IProfile) — kód produktu, ne knihovny
LedStripProfile s_profile(&s_executor);

// 4. Runtime — spojuje všechno dohromady
idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
\`\`\`

## Co dělá setup()

\`\`\`cpp
void setup() {
    // HAL: logy jdou do /dev/null, zatímco Improv vlastní Serial
    idryer::hal::initArduinoHal(nullptr);

    // Obnovit uložené přihlašovací údaje WiFi
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    }

    // Vygeneruj serial z MAC, pokud dosud není přítomen
    s_credentials.seedSerialFromMac();

    // Registruj obsluhy příkazů
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    // Volitelně: reaguj na přechody stavového stroje
    s_cloud.setStateChangeCallback([](auto prev, auto, void*) {
        if (prev == idryer::cloud::CloudState::WifiConnecting)
            configTime(0, 0, "pool.ntp.org", "time.google.com");
    }, nullptr);

    // Automatické claim pro samostatná zařízení
    s_cloud.setUnclaimedCallback([](void*) {
        s_cloud.requestClaim();
    }, nullptr);

    // Spustit runtime
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();     // CloudStateMachine + IProfile::loop()
    // ... logika produktu (senzory, telemetrie)
}
\`\`\`

## Pravidla montáže

- Všechny objekty knihovny jsou statické (\`static\`). Žádné \`new\` nebo \`malloc\` pro objekty nejvyšší úrovně.
- \`runtime.begin()\` se volá poslední v \`setup()\`, po registraci všech handlerů.
- \`runtime.loop()\` se volá první v \`loop()\`.
- Objekty produktu (senzory, telemetrie) se vytváří samostatně a připojují se přímo k \`s_mqtt\` — runtime o nich neví.

## Příklad: Storage Link

Úplný kořen kompozice Storage Link je v \`src/main.cpp\` v repozitáři iDryer-Storage (publikován samostatně).

Vrstvy zařízení v pořadí montáže:

| Vrstva | Objekty | Zdroj |
|--------|---------|-------|
| Platforma | \`s_wifiStore\`, \`s_wifi\`, \`s_credentials\`, \`s_http\` | \`idryer-core\` |
| Cloud | \`s_api\`, \`s_mqtt\`, \`s_cloud\`, \`s_dispatcher\` | \`idryer-core\` |
| Zařízení | \`s_executor\`, \`s_profile\` | \`src/storage/led_strip/\` |
| Runtime | \`s_runtime\` | \`idryer-core\` |
| Senzory | \`s_sensor\`, \`s_telemetry\` | \`src/storage/sensors/\`, \`src/storage/telemetry/\` |

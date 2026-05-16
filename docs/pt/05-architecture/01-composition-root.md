# Raiz de composição

O produto cria todos os objetos da biblioteca em \`main.cpp\` como variáveis estáticas e passa dependências através de construtores. Sem fábricas, sem registros globais — apenas montagem explícita.

## Ordem de criação de objetos

As dependências são construídas de baixo para cima: primeira camada de plataforma, depois pilha de nuvem, depois tempo de execução.

\`\`\`cpp
// 1. Camada de plataforma
idryer::ArduinoWifiStore       s_wifiStore;      // NVS: SSID/password
idryer::ArduinoWifiManager     s_wifi;           // Gerenciamento WiFi
idryer::ArduinoCredentialStore s_credentials;    // NVS: serial/token/deviceId
idryer::ArduinoHttpClient      s_http;           // TLS HTTP para provisionamento

// 2. Pilha de nuvem
idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
idryer::MqttClient               s_mqtt;
idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
idryer::ActionDispatcher         s_dispatcher;

// 3. Perfil de produto (implementa IProfile) — código do produto, não da biblioteca
LedStripProfile s_profile(&s_executor);

// 4. Tempo de execução — vincula tudo
idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
\`\`\`

## O que setup() faz

\`\`\`cpp
void setup() {
    // HAL: logs vão para /dev/null enquanto Improv possui Serial
    idryer::hal::initArduinoHal(nullptr);

    // Restaurar credenciais WiFi salvas
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    }

    // Gerar série a partir do MAC se ainda não estiver presente
    s_credentials.seedSerialFromMac();

    // Registrar manipuladores de comando
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    // Opcional: reagir a transições de máquina de estado
    s_cloud.setStateChangeCallback([](auto prev, auto, void*) {
        if (prev == idryer::cloud::CloudState::WifiConnecting)
            configTime(0, 0, "pool.ntp.org", "time.google.com");
    }, nullptr);

    // Reivindicação automática para dispositivos autónomos
    s_cloud.setUnclaimedCallback([](void*) {
        s_cloud.requestClaim();
    }, nullptr);

    // Iniciar o tempo de execução
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();     // CloudStateMachine + IProfile::loop()
    // ... lógica do produto (sensores, telemetria)
}
\`\`\`

## Regras de montagem

- Todos os objetos da biblioteca são estáticos (\`static\`). Sem \`new\` ou \`malloc\` para objetos de nível superior.
- \`runtime.begin()\` é chamado por último em \`setup()\`, depois de todos os manipuladores serem registrados.
- \`runtime.loop()\` é chamado primeiro em \`loop()\`.
- Objetos de produto (sensores, telemetria) são criados separadamente e conectados diretamente a \`s_mqtt\` — o tempo de execução não os conhece.

## Exemplo: Storage Link

A raiz de composição completa de Storage Link está em \`src/main.cpp\` no repositório iDryer-Storage (publicado separadamente).

Camadas de dispositivo em ordem de montagem:

| Camada | Objetos | Fonte |
|--------|---------|-------|
| Plataforma | \`s_wifiStore\`, \`s_wifi\`, \`s_credentials\`, \`s_http\` | \`idryer-core\` |
| Nuvem | \`s_api\`, \`s_mqtt\`, \`s_cloud\`, \`s_dispatcher\` | \`idryer-core\` |
| Dispositivo | \`s_executor\`, \`s_profile\` | \`src/storage/led_strip/\` |
| Tempo de execução | \`s_runtime\` | \`idryer-core\` |
| Sensores | \`s_sensor\`, \`s_telemetry\` | \`src/storage/sensors/\`, \`src/storage/telemetry/\` |

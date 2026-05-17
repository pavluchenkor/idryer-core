# プリンター統合

統合モジュールにより、iDryer/iHeater デバイスは第三者システムに接続できます：Home Assistant、Bambu Lab（LAN）、Moonraker/Klipper。別途インクルード：

```cpp
#include <idryer_integrations.h>
```

**統合はオプション モジュール。** Storage Link は使用しません。iDryer LINK と iHeater LINK 向けに実装されています。

## LinkIntegrationsManager

モジュールのメインクラス。一度に1つのアクティブな統合を管理します。プロダクトの `CommandHandler` を通じて接続され、MQTT とローカル WS に使用される同じハンドラーです。

```cpp
LinkIntegrationsStore intStore;
idryer::cloud::LinkIntegrationsManager intManager(&s_mqtt, &intStore);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "link_integration") == 0) {
        intManager.handleLinkIntegrationCommand(data); return;
    }
    if (strcmp(cmd, "bambu_apply") == 0) {
        intManager.handleBambuApplyCommand(data); return;
    }
    // ... other product commands ...
}

// in setup():
runtime.setCommandHandler(handleCommand);
local.setCommandSink(handleCommand);
intManager.begin(); // after runtime.begin()
// in loop(): intManager.loop();
```

マネージャーは `LinkIntegrationsStore` を通じて NVS 内の3つすべての統合の設定を格納します。アクティブな統合を切り替えるには以下のコマンドで行います：

```json
{"active": "bambu"}     // or "ha", "moonraker", "none"
```

状態は `idryer/{serial}/integrations/status`（保持）に変更時と30秒ごとに発行されます。

## Bambu Lab

`BambuClient` はローカル ネットワーク上のプリンターに MQTT 経由で接続します（TLS、ポート 8883、自己署名証明書、`setInsecure`）。

デバイスタイプに応じて2つの動作モード：

| モード | DeviceType | 動作 |
|------|-----------|-----------|
| **Writer** | Dryer | `bambu_apply` で `ams_filament_setting` をプリンターに送信 |
| **Reader** | Heater / IHeaterLink | `device/{printerSerial}/report` をサブスクリプション、プリンター状態をコールバックに渡す |

接続パラメータ：

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

再接続は1秒から60秒へのエクスポーネンシャル バックオフを用いて行われます。

コールバック：

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — HA MQTT ブローカーへの接続（HA クラウドではなく、組み込み HA MQTT サーバー）。

`link_integration` コマンドで設定：

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

アダプターは mDNS ホスト検出（文字列 `homeassistant.local`）と直接IP接続をサポートします。バックオフ付き再接続。

`HaMqttClient` は `intManager.haMqttClient()` を通じて公開 — プロダクトはそれを通じて HA エンティティを発行できます。

デバイスはクライアント ID を設定する必要があります：

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` は WebSocket（`ws://host:port/websocket`）経由で接続し、JSON-RPC 2.0 を使用して Klipper オブジェクトをサブスクリプション。

主な使用例 — iHeater：`gcode_macro VIRTUAL_CHAMBER` を通じてチャンバー目標温度を受け取る。

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

クライアントは `gcode_macro VIRTUAL_CHAMBER`、`print_stats`、`display_status`、温度センサーなど Klipper オブジェクトをサブスクリプション。

コールバック：

```cpp
intManager.setVirtualChamberCallback([](const VirtualChamberData& vc) {
    // vc.target — chamber target temperature
    // vc.available — VIRTUAL_CHAMBER object visible in Klipper
});

intManager.setMoonrakerStatusCallback([](const MoonrakerStatus& s) {
    // s.printerState, s.nozzleTemp, s.progress, ...
});
```

## 制限

- 一度に1つのアクティブな統合。切り替えはアトミック：古い方が停止し、新しい方が開始します。
- デバイスごとに1つの `BambuClient` インスタンス（静的ポインターを通じたシングルトン）。
- `LinkIntegrationsStore` は設定を NVS に格納 — 設定はリブート間で保持されます。
- デバイスは正しい Bambu モード選択のためにそのタイプを指定する必要があります：
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // or Heater, IHeaterLink
  ```

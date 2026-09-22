---
title: "デバイスカード: card マニフェスト"
description: "idryer-core のファームウェアが s_link.card() で自分のカードを記述する方法: センサー、起動パラメータ付きのアクション、ポータルに送られる内容、ポータルとアプリが描画する内容。"
---

# デバイスカード: card マニフェスト

ポータルとモバイルアプリは、どのデバイスのカードもその **card マニフェスト** から組み立てます。マニフェストはファームウェアが自分自身について公開する記述で、何を表示し、何を操作できるかを示します。新しいデバイスタイプに、ポータルやアプリのコードは必要ありません。

マニフェストは `iDryer::Link` ファサード（`s_link.card()`）の一部です。低レベルのランタイム（`IdryerRuntime`）はマニフェストを公開しません。

メニューとカードは別物です。メニューはデバイスの設定の鏡で、ポータルで変更した値はデバイスのメモリに書き込まれます。カードは測定値を表示し、操作を開始します。起動パラメータは 1 回のコマンドと一緒に送られ、メニューには書き込まれません。起動パラメータは、範囲とデフォルト値をメニュー項目から取ることができます。

---

## 仕組み

```text
ファームウェア: s_link.card() の宣言
   │  コアが JSON を組み立て、idryer/{key}/card に retained (QoS 1) で公開
   ▼
ポータルのバックエンド: マニフェストを検証（上限、許可された型とフィールド）して保存
   ▼
ポータルとアプリ: カードを描画
   │  ユーザーがボタンを押す
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
コアが "card.<id>" をあなたのコールバックに渡す
```

コアは MQTT 接続が確立した後にマニフェストを公開し、宣言や、宣言が依存するメニュー項目が変わると再公開します。

---

## エンティティ: 何を表示するか

エコシステムの語彙にあるセンサーは `Config` のフラグから追加されます。宣言は不要です:

| `Config` フラグ | カードのセル |
|---|---|
| `hasAirTemp` | 空気温度 |
| `hasAirHumidity` | 湿度 |
| `hasHeaterTemp` | ヒーター温度 |
| `hasHeater` | ヒーター出力 |
| `hasFan` | ファン オン / オフ |
| `hasServo` | ダンパー 開 / 閉 |
| `hasWeight` | 重量モジュール（topic `weights`） |
| `hasRfid` | ユニットに RFID リーダーがある |

独自の値: テレメトリに追加し、JSON パス付きのセンサーを宣言します:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

シンプルな操作部品もエンティティです: `button`、`number`、`select`。ユーザーが値を変えるとすぐに送信され、コアがコールバックを呼びます:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## アクション: 起動パラメータ付きの操作

アクションはデバイスの操作です: 乾燥の開始、加熱、照明の点灯、停止。アクションには **モード**（アクション後のユニットのモード、`status.units[].mode`）と、意味（`purpose`）を持つ **パラメータ** があります。何を表示するかは、カードがユニットの現在のモードで決めます:

- ユニットのモードがあるアクションのモードと一致 → デバイスはその作業中: セッションブロックと停止ボタン（モード `IDLE` のアクション）;
- それ以外 → 起動フォーム。起動アクションが複数あればモード切り替え。

例: ESP32 1 台で作る加温式の保管キャビネット。目標温度はメニュー項目 `target_temp`（30〜50 °C、デフォルト 45）です。

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // すでに 30..50 の範囲内
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // メニューの値をカードが読むキャッシュへ
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

コールバックが受け取るもの:

- 数値はユニットの範囲に収められ、欠けている数値はデフォルト値に置き換えられる;
- `unit` — `unitId` から得たユニットのインデックス（`"U1"` → 0）。デバイスにないユニット宛てのコマンドは無視される;
- 起動後は `status.mode` を設定して `publishStatusNow()` を呼ぶ。ステータスを見てカードがセッションブロックに切り替わる。

### アクション API

| 呼び出し | 動作 |
|---|---|
| `card.action(id, mode, cb)` | アクション。`mode` — 実行後のユニットのモード、`nullptr` — モードは変わらない |
| `.param(id, purpose, MENU_ID)` | 数値: 範囲、ステップ、デフォルト値、単位をメニュー項目から取る |
| `.param(id, purpose, min, max, step, def[, unit])` | 独自の範囲を持つ数値 |
| `.ceiling(MENU_ID)` | 直前のパラメータの上限を、そのメニュー項目の値にする（例: 最大空気温度） |
| `.stages(id, "stages", MENU_ID)` | プロファイルの段階 `[{temperature, ramp, hold}]`、秒。段階の温度はメニュー項目の範囲内 |
| `.select(id, purpose, options, count[, def])` | リストからの選択。リスト外の値はデフォルトに置き換え |
| `.color(id, purpose, "#FFFFFF")` | 色 `#RRGGBB` |
| `.deviceClass("identify")`、`.deviceClass("clear_errors")` | ヘッダーの常設ボタン: デバイスの呼び出し、エラーのクリア |
| `.name("ru", "…").name("en", "…")` | カードがモードを知らないときのアクション名 |

`purpose` の値: `target_temperature`、`target_humidity`、`duration`（分、0 — 無制限）、`stages`、`start_stage`（0 から）、`effect`、`rgb_color`。カードはこれをフィールドのラベルとポータルの機能に使います。乾燥プリセットはモード `DRYING` のアクションの `target_temperature` と `duration` を埋め、乾燥プロファイルは `stages` を埋めます。

コールバックは関数、またはキャプチャなしのラムダです。`name` の文字列と選択肢はポインタで保持されるため、リテラルか静的配列を使ってください。

---

## ポータルに送られる内容

上のキャビネットは次を公開します（`Config`: `hasAirTemp`、`hasAirHumidity`、`hasHeaterTemp`、`hasHeater`、`hasFan`）:

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` と `default` はメニュー項目から来ています。`.ceiling()` を使うと、パラメータに `max_by`（上限を切り下げたメニュー項目のタイトル）も付き、値が上限を超えたときにカードがその名前を示します。

---

## ポータルとアプリが描画する内容

スクリーンショットではなく図です。待機中:

```text
┌─ DIY Storage Cabinet ─────────── [アイドル] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                │  ← 温度、湿度、ヒーター
│ | 0 %     | オフ            |                │  ← 出力、ファン（グレー: 0 / オフ）
│ [温度 45 °C           ]  [保管]              │  ← アクション "storage"
└──────────────────────────────────────────────┘
```

起動後、デバイスはモード `STORAGE` を報告し、同じカードがセッションブロック（目標温度と保管の経過時間）と停止ボタン（モード `IDLE` のアクション）を表示します。

両方に共通のルール:

- 値がないセルはグレー。出力は 0 のときもグレー。ファンとダンパーはオフ / 閉のときグレー;
- 範囲外の値は置き換えられない。起動はブロックされ、フォームの下に理由が表示される;
- マニフェストにアクションがあるファームウェアでは、呼び出しとエラークリアのボタンは `identify` / `clear_errors` のアクションを宣言したときだけ表示される。

カードが表示される場所:

| | ポータル | アプリ |
|---|---|---|
| iDryer 製品ではないデバイスタイプ | マニフェストから作るダッシュボードのカード（アクション付き）。デバイスページ — 測定値とアクション（マニフェストにアクションがある場合） | ホーム — 測定値。アクション — デバイスページ |
| `deviceType = Dryer` | 同じマニフェストのアクションを持つ乾燥機カード | ホーム — 監視。アクション — デバイスページ |

---

## 上限

| | コア | ポータルのバックエンド |
|---|---|---|
| エンティティ | 宣言 16 + 自動 | 32 |
| layout の行 | 8 | 16 |
| 1 行の id | 4 | 4 |
| アクション | 8 | 8 |
| アクションのパラメータ | 4（`IDRYER_CARD_MAX_PARAMS`） | 6 |
| `name` の言語 | `ru`、`en` | 最大 4 |

`id`: 小文字のラテン文字、数字、`_`、最大 24 文字。エンティティとアクションは同じ名前空間を共有します。マニフェストのドキュメントは 4096 バイトまでで、収まらない場合、コアはログに `manifest overflows` と出力して公開しません。

フォーマットそのもの — `contracts/mqtt_contract.yaml`、セクション `mqtt_only`、`suffix: card`。

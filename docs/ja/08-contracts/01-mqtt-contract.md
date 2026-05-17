# MQTTコントラクト

ファイル `contracts/mqtt_contract.yaml` は `idryer-core` のMQTTインターフェースの信頼できる情報源です。

## スコープ

このコントラクトは **`idryer-core` の `MqttClient` が実装するもののみ** を記述します:

- ライブラリが公開できるトピック
- ライブラリが受け入れてルーティングするコマンド

フルプラットフォームインターフェース (すべてのバックエンドコマンド、すべてのデバイスタイプを含む `drying`, `storage`, `profile`, `rfid` など) は `contracts/portal_backend_status.md` にあります — これは [プラットフォームリファレンス] です。

## デバイストピック (デバイス → バックエンド)

| サフィックス | リテイン | ステータス |
|--------|----------|--------|
| `info` | はい | 実装済み |
| `telemetry` | いいえ | 実装済み |
| `status` | はい | 実装済み |
| `config` | いいえ | 実装済み |
| `config/delta` | いいえ | 実装済み |
| `events` | いいえ | 実装済み |
| `integrations/status` | はい | 実装済み |
| `offline` (LWT) | いいえ | 実装済み |

## コマンド (バックエンド → デバイス)

| サフィックス | ハンドラ | ステータス |
|--------|---------|--------|
| `commands/ping` | `IdryerRuntime` (組み込み) | 実装済み |
| `commands/invoke` | 製品 `CommandHandler` (推奨); フォールバック → `ActionDispatcher` | 実装済み |
| `commands/set` | 製品 `CommandHandler` (推奨); フォールバック → `ActionDispatcher` | 実装済み |
| `commands/link_integration` | `LinkIntegrationsManager` via `CommandHandler` | 実装済み |
| `commands/bambu_apply` | `LinkIntegrationsManager` via `CommandHandler` | 実装済み |
| その他すべて | 製品 `CommandHandler` | 製品定義 |

## 変更ルール

`idryer-core` のMQTTプロトコルへの変更は同時に以下に影響を与える必要があります:

1. `contracts/mqtt_contract.yaml`
2. ライブラリコード (`mqtt_client.h/.cpp`)
3. ポータル / バックエンドコード

最初にコントラクトを更新してから、コードを更新してください。

## 互換性

- ペイロードへの新しいオプショナルフィールドの追加は安全です。
- 既存フィールドの名前変更にはファームウェア、ポータル、コントラクトの同時更新が必要です。
- `info` および `config` ペイロードは製品によって定義されます — `idryer-core` はそれらを検証しません。

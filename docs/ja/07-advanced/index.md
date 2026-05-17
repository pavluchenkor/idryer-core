# インターナルス

このセクションはファサードを超えて進んだ人のためです。`iDryer::Link` がニーズをカバーしている場合、ここに来る必要はありません。

これは、内部ライブラリ コンポーネントについて説明します：デバイス コーディネーター、UART トランスポート レイヤー、プラットフォーム抽象化、およびプロファイル モデル。

- [ランタイム](01-runtime.md) — `IdryerRuntime`、エントリーポイント `begin()` / `loop()`
- [UART](02-uart.md) — デュアル MCU デバイス用のバイナリ フレーム プロトコル
- [統合](03-integrations.md) — Home Assistant、Bambu Lab、Moonraker/Klipper
- [Arduino プラットフォーム](04-platform-arduino.md) — WiFi、NVS、OTA インターフェース
- [プロファイル](05-profiles.md) — `IProfile` モデルとデバイス動作

# プロトコルとしてのメニュー: menu.yaml ↔ mqtt_contract.yaml ↔ ポータル

---

## 3つのファイル — 3つの役割

| ファイル | オーナー | 説明 |
|------|-------|-----------|
| `src/menu/menu.yaml` | 自社製品 | デバイスメニュー: パラメータ、アクション、構造 |
| `contracts/mqtt_contract.yaml` | idryer-core | 既知の意味のリスト: 各 `role:` の意味とポータルでの表示方法 |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | 生成 | ポータル用TypeScriptタイプ |

**`role:`** — メニュー項目のセマンティック名。ファームウェアは "I have button number 35" ではなく "I have `iheater.heat_start`" と言います。これはデバイスとポータル間の安定したコントラクトです — ファームウェアの内部名は変更できますが、`role:` は固定です。

**ウィジェット** — ポータルがこの項目をどのように表示するか: ボタン、スライダー、トグル、または複雑なコンポーネント (カラーピッカー、プロファイルエディタ)。`role:` を経由してコントラクトで決定されます。ファームウェアではありません。

`role:` を持つメニュー項目はポータルに表示されます。`role:` なし — プライベート、デバイスディスプレイにのみ表示されます。

---

## 1. ファームウェアビルド (`pio run`)

`menu.yaml` → `pre_gen_menu.py` はコントラクトの `canonical_roles` に対して各 `role:` を検証 → 役割が不明な場合、ビルドはエラーで失敗し、有効な役割のリストが表示されます → `menu_gen.py` はC++ファイルを `src/menu/` に生成

検証はビルドステップに組み込まれています — 存在しない役割を無視して使用することは物理的に不可能です。

## 2. ポータル用TypeScriptの更新 (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` が `mqtt-api.types.ts` を生成 → ファイルが `frontend-v2/src/contracts/` にコピーされます

コントラクトが変更されたときに手動で実行してください。結果をコミットしてください。

## 3. ランタイム: デバイス ↔ ポータル

デバイス接続 → MQTTトピック `config` にメニューを公開 → ポータルはフィールド `r:` を持つ各項目を読み取る → `CanonicalRoles[r].widget` を検索 → `WIDGET_REGISTRY` からウィジェットをレンダリング。

パラメータ (`min`, `max`, `val`) はメニュー項目自体から来ます — ファームウェアは現在の値を知っています。

---

## ポータルダッシュボードに新しいアクションを追加する方法

`role:` は自由形式フィールドではありません。値はコントラクトの `canonical_roles` の閉じたリストから来る必要があります。その場で役割を作成することはできません — ビルドは失敗します。利用可能な役割は `contracts/mqtt_contract.yaml` → `canonical_roles` セクション、または `menu.template.yaml` に表示されます。

**1. コントラクトから役割を選択します。** 適切なものがない場合 — 最初に `mqtt_contract.yaml` → `canonical_roles` に追加してから、`regen.sh` を実行します:

```yaml
canonical_roles:
  my.action: { type: action, widget: button }
```

**2. `menu.yaml` に項目を追加:**

```yaml
- id: my_action
  type: action
  role: my.action
  title: { ru: "МОЁ ДЕЙСТВИЕ", en: "MY ACTION" }
```

**3. ファームウェアで処理します (`main.cpp`)**:

```cpp
if (action == "my.action") { /* do the thing */ }
```

`pio run` → 検証 → C++ → ファームウェアは `r: "my.action"` を公開 → ポータルはボタンをレンダリング。

---

## 設定 (NVSパラメータ) を追加する方法

```yaml
- id: my_param
  type: value
  role: my.param        # ポータルに表示する場合のみ; 表示専用の場合は省略
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVSキー (≤ 15文字)
  persist: true
  scope: global
  default: 50
```

`bind` = NVSキー。`persist: true` = 再起動後も値は保持されます。
ポータルは `commands/set { "id": <id>, "val": <value> }` で値を変更します。

---

## すべきでないこと

- `menu.yaml` に `widget:` を追加しないでください — ウィジェットはファームウェアではなく、`role:` を経由してコントラクトで決定されます
- `mqtt-api.types.ts` を手動で編集しないでください — `regen.sh` で生成されます
- 新しいアクションの `Config.hasXxx` フラグに触れないでください — これらはテレメトリ (センサー、状態) のみです

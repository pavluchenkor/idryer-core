# ウィジェットと新しいデバイスを追加する

完全なサイクル: リポジトリのフォークからマージされたPRまで。ファームウェア、コントラクト、Reactウィジェット、ポータルテストをカバーしています。

ファームウェアのみが必要な場合 (新しいウィジェットは不要) — [01-add-new-product.md](01-add-new-product.md) を参照してください。

---

## 前提条件

- Python 3.9+ (`pip install pyyaml jsonschema` 付き)
- Node.js 18+
- PlatformIO CLI
- UIKitテスト用iDryerポータルへのアクセス

---

## ステップ1. フォークとクローン

1. GitHub上で `idryer-core` リポジトリをフォークします。
2. ローカルでフォークをクローンします:

    ```bash
    git clone https://github.com/<your-username>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. コントラクトが現在の状態で検証を通過することを確認します:

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## ステップ2. コントラクトを編集

すべての変更は `contracts/mqtt_contract.yaml` に入ります。すべてを1つの変更セットに保ちます。

!!! warning
    `_generated/` のファイルを編集しないでください — ジェネレーターによって上書きされます。

### 2a. 機能語彙 (新しいペリフェラルタイプ)

デバイスに新しいハードウェアタイプがある場合 (例えば、CO2センサー)、`capability_vocabulary` セクションにエントリを追加します:

```yaml
capability_vocabulary:
  co2:
    description: "CO2 sensor (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

これにより、次の再生成で `iDryer::Config` に `hasAirCo2: bool` フィールドが自動的に追加されます。

### 2b. 正規役割 (新しい役割 + ウィジェット)

デバイスが新しいメニュー項目を公開する場合、`canonical_roles` に役割を登録します:

```yaml
canonical_roles:
  co2.read:
    type: float
    widget: Co2Display
    unit: ppm
    labels:
      ru: "CO₂"
      en: "CO₂"
```

`widget` 値は、ステップ5で作成するReactコンポーネントの名前です。

### 2c. アクション呼び出し (ウィジェットがコマンドを送信する場合)

ウィジェットがデバイスでアクションをトリガーする場合、`invoke_actions` で説明します:

```yaml
invoke_actions:
  my_device:
    co2.calibrate:
      description: "Start CO2 sensor calibration"
      args:
        targetPpm:
          type: uint16
          description: "Reference CO2 value (ppm)"
          required: true
```

### 2d. デバイスプロファイル (新しいデバイスタイプ)

`device_profiles` にプロファイルを追加します:

```yaml
device_profiles:
  my_device:
    description: "My device"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

機能値はステップ2aで定義されている `capability_vocabulary` から来ます。

---

## ステップ3. 検証と再生成

```bash
cd contracts
./regen.sh
```

フラグ:

| フラグ | 効果 |
|---|---|
| (なし) | 検証 + すべてのジェネレーター + ポータルへのコピー |
| `--firmware-only` | ファームウェアジェネレーターのみ、ポータルコピーをスキップ |
| `--help` | ヘルプを表示 |

成功すると、`_generated/` は以下で更新されます:

- `uart_protocol.h`, `mqtt_topics.h` — C++ヘッダー
- `iDryer_api.h` — Config/DeviceTypeファサード
- `mqtt-api.types.ts` — TypeScriptタイプ
- `scaffolds/my_device/` — PlatformIOプロジェクトスケルトン
- ポータル上: `src/components/widgets/` 内のファイル

`regen.sh` がエラーで終了した場合、続行する前に問題を修正してください。

---

## ステップ4. ファームウェアを実装

生成されたスケルトンプロジェクトを使用します:

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

`src/main.cpp` のTODOセクションを入力します:

- `onOnline()` — NVSから設定を読み込み、ハードウェアを初期化します。
- `loop()` — センサーをポーリングし、`s_runtime.publishTelemetry(tel)` を呼び出します。
- `buildInfoJson()` — ジェネレーターからの機能によってすでに入力されています。
- `onInvoke()` — `co2.calibrate` を処理します。

詳細については、[01-add-new-product.md](01-add-new-product.md) を参照してください。

---

## ステップ5. Reactウィジェットを作成

ウィジェットは `contracts/widgets/` に存在し、`regen.sh` によってポータルにコピーされます。

!!! note
    `portal/src/components/widgets/` でウィジェットを直接編集しないでください — 次の `regen.sh` の実行時に上書きされます。`contracts/widgets/` でのみ編集してください。

### ウィジェットファイルを作成

```tsx
// contracts/widgets/Co2Display.tsx
import type { WidgetProps } from "./widget-props";

export function Co2DisplayWidget({ device }: WidgetProps) {
  const unit = device.units[0];
  const co2 = unit?.co2Ppm ?? null;
  return (
    <div style={{ padding: "8px 16px" }}>
      {co2 !== null ? `${co2} ppm` : "—"}
    </div>
  );
}
```

### index.ts に登録

```ts
// contracts/widgets/index.ts
export { Co2DisplayWidget } from "./Co2Display";
```

### widget-registry.tsx に登録 (ポータル上)

次の `regen.sh` の実行後、ファイルは `portal/src/components/widgets/Co2Display.tsx` に表示されます。手動で `widget-registry.tsx` にエントリを追加します:

```tsx
import { Co2DisplayWidget } from "./Co2Display";

export const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  // ...
  Co2Display: Co2DisplayWidget,
};
```

---

## ステップ6. UIKitでテスト

`portal/src/pages/UiKitPage.tsx` を開き、**Device Dashboard Widgets** グループ内にモックデータを含むセクションを追加します:

```tsx
<KitSection title="Co2Display">
  <Co2DisplayWidget device={MOCK_DEVICE} item={MOCK_CO2_ITEM} socket={null} />
</KitSection>
```

ポータルをローカルで開き、`/uikit` に移動します — ウィジェットはログインなしでレンダリングされます。

---

## ステップ7. PRチェックリスト

PRを送信する前に、以下を確認してください:

- [ ] `./contracts/regen.sh` がエラーなしで完了する
- [ ] `_generated/*` がコミットされている (`.gitignore` に含まれていない)
- [ ] `contracts/widgets/` — 新しいウィジェットファイルが追加されている
- [ ] `contracts/widgets/index.ts` — ウィジェットがエクスポートされている
- [ ] ポータル上の `widget-registry.tsx` — ウィジェットが登録されている
- [ ] `/uikit` でウィジェットがコンソールエラーなしでレンダリングされる
- [ ] `_generated/scaffolds/my_device/` のスケルトンが機能を正しく反映している
- [ ] PR説明: デバイスの目的、機能、ウィジェット名を記載しています

`idryer-core` リポジトリの `main` ブランチに対してPRを送信します。

---

## 1つのPRですべての変更

| ファイル | 変更タイプ |
|---|---|
| `contracts/mqtt_contract.yaml` | 信頼できる情報源 |
| `contracts/_generated/*` | 自動生成 — 全体をコミット |
| `contracts/widgets/MyWidget.tsx` | 新しいファイル |
| `contracts/widgets/index.ts` | +1エクスポート行 |
| *(ポータル、`regen.sh` 後)* | `src/components/widgets/MyWidget.tsx` — コピー |
| *(ポータル、手動)* | `src/components/widgets/widget-registry.tsx` — +1エントリ |
| *(ポータル、手動)* | `src/pages/UiKitPage.tsx` — KitGroup内の +1セクション |

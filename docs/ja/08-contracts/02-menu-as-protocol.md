# プロトコルとしてのメニュー: menu.yaml ↔ mqtt_contract.yaml ↔ ポータル

---

## 3 つのファイル、3 つの役割

| ファイル | 所有者 | 記述する内容 |
|------|-------|-----------|
| `src/menu/menu.yaml` | あなたの製品 | デバイスのメニュー: パラメータ、アクション、構造 |
| `contracts/mqtt_contract.yaml` | idryer-core | 既知の意味の一覧: 複数言語のラベルを持つ `canonical_roles` |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | 生成物 | ポータル用の TypeScript 型 |

**`role:`** — メニュー項目の意味的な名前です。ファームウェアは「35 番の項目がある」ではなく「`iheater.heat_temp` がある」と伝えます。ファームウェア内部の名前は変わっても、`role:` は固定です。

メニューはデバイス設定の鏡です。ファームウェアは `menu.yaml` のすべての項目を、`role:` の有無にかかわらず公開します。ポータルは各項目をその型（値、トグル、アクション、サブメニュー）で描画します。`role:` は項目にユーザーの言語のコントラクト由来のラベルを与えます。`role:` がなければ、ポータルはデバイスが送った名前を表示します。

デバイスカードはメニューから作られません。カードが何を表示し、どの操作を開始するかは card マニフェストが記述します — [デバイスカード: card マニフェスト](../09-add-product/02-add-widget.md) を参照してください。カードのアクションは、パラメータの範囲とデフォルト値をメニュー項目から取ることができます。

---

## 1. ファームウェアのビルド（`pio run`）

`menu.yaml` → `menu_gen.py` が各 `role:` をコントラクトの `canonical_roles` と照合 → 未知のロールがあればエラーと有効なロールの一覧を出してビルド失敗 → ジェネレーターが C++ ファイルを `src/menu/` に書き出す。

## 2. ポータル用 TypeScript の更新（`regen.sh`）

`mqtt_contract.yaml` → `gen_ts_types.py` が `mqtt-api.types.ts` とロールのラベル `roles.{lang}.json` を生成 → ファイルがポータルにコピーされる。

コントラクトを変更したときに実行し、結果をコミットします。

## 3. 実行時: デバイス ↔ ポータル

ファームウェアがメニューを `config` トピックに公開（`get_config` コマンドを受けて製品コードが行う。iDryer 製品はオンラインになったときも公開）→ ポータルのバックエンドが保存 → ポータルが `GET /devices/:id/menu-config` で取得 → 各項目を型 `t`（`val`、`tog`、`act`、`sub`）で描画。ラベルは `canonical_roles[r].labels[lang]`、次に英語、最後にデバイスからの名前 `n`。

パラメータ（`min`、`max`、`val`）はメニュー項目そのものから来ます。現在値を知っているのはファームウェアです。

ポータルは `commands/set { "id": <id>, "val": <value> }` で値を変更します。

---

## 設定（NVS パラメータ）の追加方法

```yaml
- id: my_param
  type: value
  role: my.param        # 任意: コントラクトのラベル
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS キー（15 文字以内）
  persist: true
  scope: global
  default: 50
```

`bind` = NVS キー。`persist: true` = 値は再起動後も残ります。

`role:` は自由入力のフィールドではありません。値はコントラクトの `canonical_roles` にあるものでなければならず、そうでなければビルドは失敗します。一覧は `contracts/mqtt_contract.yaml` → `canonical_roles` または `menu.template.yaml` にあります。新しいロールはまずコントラクトに追加し、その後 `regen.sh` を実行します。

---

## デバイスカードに操作を追加する方法

操作（開始、停止、加熱、照明）はメニュー項目ではなく、カードのアクションとして宣言します。範囲はメニュー項目から取れます:

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

詳しい説明 — [デバイスカード: card マニフェスト](../09-add-product/02-add-widget.md)。

---

## やってはいけないこと

- `menu.yaml` に `widget:` を追加しない。`canonical_roles` の `widget` フィールドは参考情報で、ポータルもアプリも読みません。
- `mqtt-api.types.ts` を手で編集しない — `regen.sh` が生成します。
- 新しいアクションのために `Config.hasXxx` フラグを変更しない — これはテレメトリ（センサー、状態）専用です。

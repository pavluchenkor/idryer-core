# idryer-core 如何運作

idryer-core 是一個針對 ESP32 的庫，處理整個雲端堆棧：通過 Improv-Serial 進行 WiFi 配置、用於將設備綁定到 idryer.org 帳戶的聲明協議、具有自動重新連接的 TLS MQTT 會話、來自門戶的命令路由和定期遙測發布。

您只需編寫特定於您的設備的內容：讀取傳感器、驅動外設。其餘一切都在庫內。

## mqtt_contract.yaml — 唯一真實來源

文件 [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 定義：

- **功能** — 每種設備類型支持的外設（加熱器、LED 條、傳感器）；
- **遙測字段** — MQTT 數據包中的字段名稱和數據類型；
- **UART 協議** — ESP32 和協處理器之間的結構；
- **TypeScript 類型** — 用於門戶前端。

代碼從此文件自動生成：

| 生成的內容 | 位置 |
|---|---|
| `iDryer::Config`（has* 標誌） | `src/_generated/iDryer_api.h` |
| MQTT 主題（C++ 常量） | `contracts/_generated/mqtt_topics.h` |
| TypeScript 類型 | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    不要手動編輯 `src/_generated/` 和 `contracts/_generated/` 中的文件 — 它們在下一次重新生成運行時會被覆蓋。

## 如何添加新外設

任何新功能的流程都是相同的 — 按鈕、CO2 傳感器、RFID 讀取器。

**1.** 在 [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 中的 `capability_vocabulary` 添加條目：

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** 運行重新生成：

```bash
cd contracts
./regen.sh
```

之後，`iDryer::Config` 將具有 `hasCo2` 字段，TypeScript 將具有 `HardwareUnitConfigCapabilities.co2`。

**3.** 在您設備的 `main.cpp` 中設置標誌：

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** 刷新設備。門戶將從 MQTT `/info` 主題讀取 `co2: true` 並自動顯示相應的 UI 塊 — 不需要門戶端更改。

對於合約中尚未包含的外設類型，打開 PR 到 idryer-core 存儲庫，在 `capability_vocabulary` 中添加條目。合併後 — 運行 `regen.sh`。

## 基於此庫構建的兩個生產產品

**iDryer Storage Link** — 帶有 WS2812B LED 條和 SHT31 溫度/濕度傳感器的 ESP32-C3。

**iHeater Link** — 帶有 RMT 輸出到 iHeater 加熱器的 ESP32-C3，具有 Bambu Lab、Klipper/Moonraker 和 Home Assistant 的集成。

兩個產品都通過 PlatformIO `lib_deps` 包括 idryer-core，並只實現其產品特定的邏輯。

## 下一步

- [01-wifi.md](01-wifi.md) — 使用 Improv-Serial 將 ESP32 連接到 WiFi。
- [../../../README.md](../../../README.md) — 庫概述和代碼生成參考。

import { useState } from "react";
import { useTranslation } from "react-i18next";
import { InvokeActions } from "../../contracts/mqtt-api.types";
import { Select } from "../ui/Select";
import type { SelectOption } from "../ui/Select";
import { CardActions } from "../device/CardActions";
import { sendDeviceInvoke } from "./widget-props";
import type { WidgetProps } from "./widget-props";

const _ledPulse = InvokeActions["storage_link"]["led.pulse"];
const LED_ANIM_VALUES = _ledPulse.animation.filter((v) => v !== "off");
type LedAnim = (typeof LED_ANIM_VALUES)[number];

const LED_ANIM_OPTIONS: SelectOption<LedAnim>[] = LED_ANIM_VALUES.map((v) => ({
  value: v,
  label: v.charAt(0).toUpperCase() + v.slice(1),
}));

export function LedPulseWidget({ device, socket }: WidgetProps) {
  const { t } = useTranslation();
  const [draftAnim, setDraftAnim] = useState<LedAnim>("solid");
  const [draftColor, setDraftColor] = useState("#ffffff");

  return (
    <>
      <div style={{ padding: "8px 16px 0", display: "flex", gap: 8 }}>
        <div style={{ flex: 1 }}>
          <Select<LedAnim> value={draftAnim} onChange={setDraftAnim} options={LED_ANIM_OPTIONS} />
        </div>
        <input
          type="color"
          value={draftColor}
          onChange={(e) => setDraftColor(e.target.value)}
          style={{
            width: 36,
            height: 36,
            padding: 2,
            borderRadius: "var(--r)",
            border: "1px solid var(--line)",
            background: "var(--paper-2)",
            cursor: "pointer",
          }}
        />
      </div>
      <CardActions cols={2}>
        <button
          className="btn btn-contained"
          onClick={() =>
            sendDeviceInvoke(socket, device.id, "led.pulse", {
              animation: draftAnim,
              color: draftColor,
            })
          }
        >
          {t("common.turnOn")}
        </button>
        <button
          className="btn btn-outlined"
          onClick={() => sendDeviceInvoke(socket, device.id, "led.pulse", { animation: "off" })}
        >
          {t("common.turnOff")}
        </button>
      </CardActions>
    </>
  );
}

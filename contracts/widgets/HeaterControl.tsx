import { useState } from "react";
import { useTranslation } from "react-i18next";
import { NumberInput } from "../device/NumberInput";
import { CardActions } from "../device/CardActions";
import { PlayArrowIcon, StopIcon } from "../ui/Icons";
import { sendDeviceInvoke, resolveRoleLabel } from "./widget-props";
import type { WidgetProps } from "./widget-props";

export function HeaterControlWidget({ device, item, socket }: WidgetProps) {
  const { t, i18n } = useTranslation();
  const [draftTemp, setDraftTemp] = useState(50);
  const [draftDuration, setDraftDuration] = useState(0);
  const isHeating = device.deviceStatus === "DRYING";
  const label = resolveRoleLabel(item, i18n.language);

  if (isHeating) {
    return (
      <CardActions cols={1}>
        <button
          className="btn btn-contained btn-error"
          onClick={() => sendDeviceInvoke(socket, device.id, "heat.stop")}
        >
          <StopIcon sx={{ fontSize: 18 }} /> {t("common.turnOff")}
        </button>
      </CardActions>
    );
  }

  return (
    <CardActions cols={3}>
      <NumberInput value={draftTemp} onChange={setDraftTemp} min={40} max={120} step={5} unit="°C" />
      <NumberInput
        value={draftDuration}
        onChange={setDraftDuration}
        min={0}
        max={480}
        step={15}
        unit={draftDuration === 0 ? "∞" : t("units.minutes")}
      />
      <button
        className="btn btn-contained"
        onClick={() =>
          sendDeviceInvoke(socket, device.id, "heat.start", {
            tempC: draftTemp,
            durationMin: draftDuration,
          })
        }
      >
        <PlayArrowIcon sx={{ fontSize: 18 }} /> {label}
      </button>
    </CardActions>
  );
}

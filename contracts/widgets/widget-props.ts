import type { Socket } from "socket.io-client";
import type { CanonicalRole } from "../../contracts/mqtt-api.types";
import { CanonicalRoles } from "../../contracts/mqtt-api.types";
import type { DeviceMenuItem } from "../../api/devices";
import type { Device } from "../../types/device";

export interface WidgetProps {
  device: Device;
  item: DeviceMenuItem;
  socket: Socket | null;
}

export function sendDeviceInvoke(
  socket: Socket | null,
  deviceId: string,
  action: string,
  args: Record<string, unknown> = {}
) {
  socket?.emit("device:command", {
    deviceId,
    command: { type: "INVOKE", unitId: "U1", payload: { action, args } },
  });
}

export function resolveRoleLabel(item: DeviceMenuItem, lang: string): string {
  if (item.r && item.r in CanonicalRoles) {
    const labels = CanonicalRoles[item.r as CanonicalRole].labels as Record<string, string>;
    return labels[lang] ?? labels["en"] ?? item.n;
  }
  return item.n;
}

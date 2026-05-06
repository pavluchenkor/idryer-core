/**
 * @file idryer_uart.h
 * @brief UART bridge layer for two-MCU iDryer devices.
 *
 * Include alongside @c idryer_core.h on the ESP32 (Link) side of a device
 * that pairs an ESP32 with an RP2040 controller over UART.
 *
 * Provides three components:
 *   - UartBridge      — bidirectional transport: frame parsing, ACK/retry, callback dispatch
 *   - uart_protocol   — shared frame types, payload structs, and constants (usable on both MCUs)
 *   - ConfigReceiver / ConfigSender — fragmented JSON config transfer over UART
 *
 * Not needed for standalone ESP32-only devices (e.g. Storage Link).
 *
 * @see UartBridge
 * @see ConfigReceiver
 * @see ConfigSender
 */

#pragma once

#include "uart/uart_protocol.h"
#include "uart/uart_bridge.h"
#include "config/config_manager.h"

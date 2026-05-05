# BLE_p2pServer_ota — STM32WBA55 Datalogger

## What This Project Is

A **datalogger firmware** for the **STM32WBA55** wireless SoC that combines:
- BLE P2P communication (LED/button control via GATT)
- BLE OTA firmware updates
- Periodic cellular (LTE/GSM) data upload to a cloud server

Base template: ST's `BLE_p2pServer_ota` example, extended with custom cellular + application logic.

---

## Hardware

- **MCU:** STM32WBA55 (integrated BLE radio)
- **Modem:** GSM/LTE module on USART2 (AT commands)
- **Debug UART:** LPUART1
- **Other peripherals:** I2C, RTC, GPDMA, ADC, RNG, GPIO for modem power control

---

## Key Files (Custom / User Code)

| File | Purpose |
|------|---------|
| `Core/Src/main.c` | Peripheral init (GPIO, UART, I2C, RTC, RNG), calls BLE stack |
| `UserApplication/application.c` | 80-second timer → triggers cellular send cycle |
| `Cellular/cellular.c` | GPIO control for modem boost power & antenna switch |
| `Cellular/command.c` | Full AT command state machine (1038 lines), TCP send to `datalogger.adarko.io:8900` |
| `STM32_WPAN/App/app_ble.c` | BLE stack init, GAP/GATT, advertising, security |
| `STM32_WPAN/App/p2p_server_app.c` | P2P service: LED control + button notify |
| `STM32_WPAN/App/ota_app.c` | OTA: receives 248-byte firmware chunks, flashes, reboots |

ST-generated HAL peripheral drivers live in `Core/Src/` and `Drivers/`. BLE middleware is in `Middlewares/ST/STM32_WPAN/`.

---

## Application Flow

1. **Boot** → init peripherals → init BLE stack (`MX_APPE_Init`) → main loop (`MX_APPE_Process`)
2. **BLE** advertises P2P + OTA services continuously
3. **Every 80 seconds** → `Send_Data_Req()` fires (task sequencer)
   - Power up cellular modem (boost GPIO via `CellularInit()`)
   - Run AT command sequence (see below)
   - Power down modem (`CellularDeInit()`) → return to low power mode
4. **OTA** (if BLE client initiates): receive 248-byte firmware chunks → write flash → reboot

---

## AT Command Sequence (`Cellular/command.c`)

20+ step state machine:

```
AT → SIM ready → CREG/CGREG (network reg) → CSQ (signal) → CPSI (network info)
→ CGDCONT (PDP context) → CNTP (NTP sync) → NETOPEN → CDNSGIP (DNS)
→ CIPOPEN → CIPSEND (74-byte payload) → CIPCLOSE → NETCLOSE → done
```

- Retry logic: 5 retries for network registration, 5 for NTP
- Response parsing: circular buffer (50 bytes) + command buffer (540 bytes)
- Server: `datalogger.adarko.io:8900` (TCP, 74-byte payload)

---

## Low Power

- LPTIM1/2 used for low-power timers
- Cellular modem powered off between send cycles
- STM32 LPM manager: `Utilities/lpm/`
- Task sequencer: `Utilities/sequencer/`

---

## Build / Edit Workflow

- **Edit code:** VSCode only
- **Compile & flash:** STM32CubeIDE only — do not attempt CLI builds
- **Peripheral config:** `BLE_p2pServer_ota.ioc` is managed by STM32CubeMX — do not hand-edit this file
- **Build configs:** Debug and Release both available in `STM32CubeIDE/`

---

## Verification

No automated test suite. To verify changes:
1. Build in STM32CubeIDE (Release config for production)
2. Flash to hardware
3. Monitor LPUART1 for debug logs
4. Monitor USART2 for modem AT command/response traffic

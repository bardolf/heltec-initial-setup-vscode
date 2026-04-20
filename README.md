# heltec-initial-setup-vscode

Working baseline for developing on a **Heltec WiFi LoRa 32 V3** (ESP32-S3) from **VS Code** using the **ESP-IDF** extension and **clangd** for IntelliSense. Starts from the ESP-IDF `blink` example, reconfigured for the on-board LED.

## Hardware

- [Heltec WiFi LoRa 32 V3 (868 MHz)](https://www.laskakit.cz/heltec-wifi-lora-32-v3-868mhz-0-96--wifi-modul/)
- On-board white user LED on **GPIO 35** (active-high)
- USB-C connects as `/dev/ttyACM0` on Linux (uses the built-in USB-Serial/JTAG of the ESP32-S3)

## Prerequisites

One-time install per machine:

1. **VS Code** with extensions:
   - `espressif.esp-idf-extension`
   - `llvm-vs-code-extensions.vscode-clangd`
2. **ESP-IDF v6.0** — install via the ESP-IDF extension's *Configure ESP-IDF Extension* command, or manually per the [official guide](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/get-started/linux-macos-setup.html).

## First-time project setup

```bash
git clone git@github.com:bardolf/heltec-initial-setup-vscode.git
cd heltec-initial-setup-vscode
```

Open in VS Code. The ESP-IDF extension will ask you to select the IDF installation; pick your v6.0.

Set the target once:

```bash
idf.py set-target esp32s3
```

(or use the target selector in the ESP-IDF status bar)

## Build / Flash / Monitor

From VS Code:

- **🔥 Build, Flash & Monitor** button in the status bar (equivalent to `idf.py build flash monitor`)

From the terminal:

```bash
idf.py build flash monitor
```

Expected log output:

```
I (xxx) example: Example configured to blink GPIO LED!
I (xxx) example: Turning the LED ON!
I (xxx) example: Turning the LED OFF!
```

The on-board white LED blinks at 1 Hz.

## Key configuration choices

The following are already baked into this repo — no menuconfig needed:

| Setting | Value | Where |
|---|---|---|
| Target chip | `esp32s3` | `sdkconfig.defaults.esp32s3` |
| LED type | GPIO (not LED strip) | `sdkconfig.defaults.esp32s3` |
| LED GPIO | `35` (Heltec V3 user LED) | `sdkconfig.defaults.esp32s3` |
| Flash method | UART (not JTAG) | `.vscode/settings.json` → `idf.flashType` |
| Serial port | `/dev/ttyACM0` | `.vscode/settings.json` → `idf.port` |
| IntelliSense | clangd (Microsoft C/C++ engine disabled) | `.vscode/settings.json` |

## Gotchas encountered

- **VS Code "Flash" asked to run OpenOCD.** Cause: `idf.flashType` defaulted to `JTAG`. Fix: set to `UART` (or use Command Palette → *ESP-IDF: Select Flash Method*).
- **LED didn't blink after first flash.** Cause: the blink example defaults to the ESP32-S3-DevKitC config (addressable WS2812 on GPIO 38). Fix: switch to plain GPIO and set GPIO 35 in `sdkconfig.defaults.esp32s3`, then delete `sdkconfig` so defaults regenerate.
- **VS Code showed red `#include` squiggles (C/C++ error 1696).** Cause: Microsoft C/C++ and clangd both active. Fix: set `C_Cpp.intelliSenseEngine` to `disabled` so clangd (which reads `build/compile_commands.json`) owns IntelliSense.
- **clangd warning "header `led_strip.h` is not used directly".** The header is only needed for the addressable-LED code path; moved the include inside `#ifdef CONFIG_BLINK_LED_STRIP`.

## Repo layout

```
main/                      # app source + Kconfig
sdkconfig.defaults         # common defaults
sdkconfig.defaults.esp32s3 # Heltec V3 overrides (LED type + GPIO)
.vscode/                   # editor + extension settings (absolute paths; re-run "Configure ESP-IDF Extension" on a new machine)
dependencies.lock          # managed_components lockfile (commit this)
CMakeLists.txt             # top-level ESP-IDF project file
```

Build artifacts (`build/`, `sdkconfig`, `managed_components/`) are gitignored and regenerated on build.

## Based on

ESP-IDF `examples/get-started/blink`.

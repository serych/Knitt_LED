# Architecture

This page describes the overall structure of **KnittLED** (ESP32 + OLED + NeoPixel LEDs + Web UI),
the responsibilities of each module, and the main data/control flows.

## Overview

KnittLED provides:

- A **Wi‑Fi provisioning portal** (fallback hotspot `KnittLED`) to configure STA credentials.
- A **web UI** for editing and knitting patterns (grid editor + knit mode).
- A **NeoPixel LED row** that displays the currently active pattern row.
- An **OLED status display** showing row and total carriage pulses.
- **Buttons / carriage sensor** to advance rows and confirm.

Data is persisted using:

- **LittleFS**: pattern files stored under `/patterns/*.json`
- **Preferences (NVS)**: configuration + Wi‑Fi credentials

## Module map

| Module | Responsibility |
|---|---|
| `main.cpp` | Wiring everything together: boot flow, mode selection, input handling, output refresh, blink warning logic |
| `WifiPortal.*` | Captive portal + network scan + storing credentials, then reboot into STA mode |
| `WebUi.*` | Web UI HTML/JS + REST-like API endpoints + file management (list/load/save/upload/download) |
| `Pattern.*` | In-memory pattern model + serialization to/from JSON |
| `AppConfig.*` | Runtime configuration + persistence to Preferences |
| `LedView.*` | Mapping active pattern row to NeoPixel strip (LED0 rightmost) + blink helper |
| `OledView.*` | OLED rendering (IP, row/total status) |
| `Buttons.*` | Debounced edge detection for physical buttons/sensor |

## Boot flow

1. Mount **LittleFS** and ensure `/patterns/` exists.
2. Load configuration from **Preferences** (`AppConfig`) and Wi‑Fi credentials.
3. Try to connect to Wi‑Fi as **STA**:
   - If connected: start the main web server (`WebUi`) and show IP on OLED.
   - If not: start **AP+portal** using `WifiPortal`, show `AP: KnittLED` on OLED.
4. After provisioning succeeds: stop portal services and **restart** to come up cleanly in STA mode.

## Control flow (knitting)

**Inputs**
- Web UI: `/api/row` (step), `/api/confirm`
- Physical buttons: Up/Down/Confirm
- Carriage sensor: acts as “Up” step and increments total pulse count

**State**
- `cfg.activeRow` is the internal row index (0 = top).
- `cfg.rowFromBottom` changes *how steps are applied* and how row number is displayed.
- `rowConfirmed[]` tracks whether the current row has been confirmed.
- `cfg.totalPulses` counts carriage sensor pulses.
- `cfg.warnBlinkActive` enables blink warning mode if carriage advances without confirmation.

**Outputs**
- `LedView::showRow()` shows pixels of active row in active/confirmed color.
- `OledView::showKnitStatus()` shows `Row:xx/yy, Tot:zz`.

## Row numbering and direction

Internally, row index `0` corresponds to the **topmost** row in the editor grid.

- If `rowFromBottom = false` (default):
  - “Row +” increases the internal row index.
  - OLED shows `activeRow + 1`.
- If `rowFromBottom = true`:
  - “Row +” decreases the internal row index (counting from the bottom).
  - OLED shows `pattern.h - activeRow`.

Row stepping is wrap-around (after last row -> first row).

## Blink warning

If **blink warning** is enabled and the carriage sensor triggers while the current row is not confirmed:

- `cfg.warnBlinkActive` is set to true
- LEDs blink until the row is confirmed or the user changes row manually

## Persistence

- Patterns are stored as JSON under `/patterns/*.json` (LittleFS).
- Configuration is stored under Preferences namespace `knittled`.

## Extension points

Common next steps:
- Mirror/invert pattern options
- Persist `rowConfirmed[]` and/or `totalPulses` across reboot
- Multiple patterns / pattern selection UX improvements
- Support different LED widths or multiple LED strips

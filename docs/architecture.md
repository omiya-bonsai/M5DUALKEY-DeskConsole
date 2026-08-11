# Architecture

Japanese: [architecture.ja.md](architecture.ja.md)

## System Overview

The USB-C connector side is treated as the front of the DualKey module in this project. From that front view, the official physical layout from left to right is Angle, Encoder, then the ESP32-S3-based Chain DualKey. The three modules form an M5Chain UART daisy chain.

```text
Front view (USB-C side)

+-----------+-----------+-----------+
| Angle     | Encoder   | DualKey   |
+-----------+-----------+-----------+

Angle -> Encoder -> DualKey (ESP32-S3)
       M5Chain UART daisy chain
```

The firmware discovers the Encoder and Angle by device type rather than assigning fixed Chain IDs. The DualKey buttons are read directly by the ESP32-S3. Encoder rotation and its button produce USB HID Consumer Control events, while Angle produces USB HID mouse-wheel events.

The three HID paths have distinct responsibilities:

```text
M5DUALKEY-DeskConsole
        |
        +--> USB HID Keyboard
        |       |
        |       v
        |    Raycast
        |       |
        |       v
        |   Audio Output Switching
        |
        +--> USB HID Consumer Control
        |       |
        |       v
        |   Volume / Mute
        |
        +--> USB HID Mouse
                |
                v
             Scroll
```

Only the DualKey keyboard shortcuts use Raycast. The ESP32 does not address ORA4 or Studio Display directly; Raycast and its configured macOS script perform the actual switch. Encoder volume/mute events and Angle mouse-wheel events go directly from USB HID to macOS without Raycast.

## Responsibilities

- **Input processing:** debounces DualKey and Encoder input, recognizes the DualKey chord, and interprets Angle position.
- **HID output:** sends DualKey keyboard shortcuts through Raycast, while Consumer Control and mouse-wheel events go directly to macOS.
- **Angle control:** calibrates center at startup, applies hysteresis, and calculates scroll timing.
- **LED state:** stores the last output selected from DualKey plus the locally toggled mute state, then renders them through a dedicated LED update function.
- **macOS automation:** maps only the three DualKey audio-output shortcuts to scripts outside the firmware.

## LED State Boundary

LED status is maintained on the M5Stack controller. There is no feedback channel from macOS, so it is not authoritative system state. Changing audio output or mute through macOS, another keyboard, or another application can make the LED display differ from the actual macOS state. A future BLE or host-feedback feature can be added around the isolated state and LED-update layer without mixing LED commands into input handling.

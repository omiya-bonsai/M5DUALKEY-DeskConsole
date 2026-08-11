# M5DUALKEY-DeskConsole

Japanese: [README.ja.md](README.ja.md)

<p align="center">
  <img src="assets/images/dualkey.jpg" width="700" alt="M5DUALKEY-DeskConsole">
</p>

M5DUALKEY-DeskConsole is a compact USB HID controller for macOS, built from M5Stack Chain Angle, Encoder, and DualKey modules.

It sends standard keyboard, consumer-control, and mouse-wheel events. Audio-output switching is delegated to Raycast (or another macOS automation tool), keeping operating-system-specific work outside the firmware.

## Features

- One-touch audio-output selection and toggle shortcuts
- Hardware volume and mute control
- Spring-centered auto-scroll with startup calibration and hysteresis
- Low-brightness RGB status indicators for audio output and mute
- Automatic discovery of Chain Encoder and Angle modules

## Hardware

The USB-C connector side is treated as the front of the DualKey module in this project. All left/right references use this viewing direction. From left to right, the official physical layout is Angle, Encoder, then DualKey.

```text
Front view (USB-C side)

+-----------+-----------+-----------+
| Angle     | Encoder   | DualKey   |
+-----------+-----------+-----------+
```

- M5Stack Chain Angle
- M5Stack Chain Encoder
- M5Stack Chain DualKey
- USB connection to macOS

## Quick Controls

| Module | Control | Action |
| --- | --- | --- |
| DualKey | Left | Select Studio Display (`Ctrl + Cmd + 2`) |
| DualKey | Right | Select ORA4 (`Ctrl + Cmd + 1`) |
| DualKey | Both | Toggle output (`Ctrl + Option + S`) |
| Encoder | Turn / press | Volume Up, Volume Down, or Mute |
| Angle | Left / center / right | Scroll Up, stop, or Scroll Down |

Only the three DualKey audio-output actions use Raycast. Encoder volume/mute and Angle scrolling are sent directly to macOS as USB HID events.

The LEDs represent firmware-maintained state, not state read back from macOS. See [Controls](docs/controls.md) for startup behavior and limitations.

## Detailed Documentation

- [Architecture](docs/architecture.md)
- [Controls and LED behavior](docs/controls.md)
- [Raycast integration](docs/raycast.md)
- [Development and build settings](docs/development.md)

## Project Status

USB HID keyboard, consumer control, Angle auto-scroll, startup calibration, and RGB status indicators are implemented. BLE HID is planned.

## License

MIT License. See [LICENSE](LICENSE).

## Maintainer

omiya-bonsai

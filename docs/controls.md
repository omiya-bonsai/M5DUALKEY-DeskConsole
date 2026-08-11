# Controls and LED Behavior

Japanese: [controls.ja.md](controls.ja.md)

The USB-C connector side is treated as the front of the DualKey module. From this front view, the physical layout from left to right is Angle, Encoder, then DualKey. All left/right controls and LED positions below are described from this viewing direction.

## DualKey

| Control | HID shortcut | Expected Raycast action |
| --- | --- | --- |
| Left key | `Ctrl + Cmd + 2` | Select Studio Display |
| Right key | `Ctrl + Cmd + 1` | Select ORA4 |
| Both keys | `Ctrl + Option + S` | Toggle ORA4 / Studio Display |

The two-key chord has priority. A short chord window prevents a near-simultaneous press from producing a single-key action first.

These are the only controls that use Raycast.

## Encoder

| Control | USB HID Consumer Control |
| --- | --- |
| Clockwise | Volume Up |
| Counter-clockwise | Volume Down |
| Press | Mute / Unmute |

Encoder events are sent directly to macOS as USB HID Consumer Control and do not pass through Raycast.

## Angle

| Position | Action |
| --- | --- |
| Left | Auto-scroll Up |
| Center | Stop scrolling |
| Right | Auto-scroll Down |

Angle scrolling is sent directly to macOS as USB HID mouse-wheel events and does not pass through Raycast.

The Angle center is calibrated during startup. Hysteresis uses separate start and stop thresholds so small movements around center do not repeatedly start scrolling. Beyond the start threshold, scroll speed follows an x^1.5 curve and increases with distance from center.

Keep the Angle physically centered while the controller starts.

## LED Status

| Internal state | LED display |
| --- | --- |
| Audio output unknown | Both DualKey LEDs off |
| Studio Display selected | Left DualKey LED white |
| ORA4 selected | Right DualKey LED blue |
| Locally tracked mute on | Encoder LED red |
| Locally tracked mute off | Encoder LED off |
| Any Angle state | Angle LED off |

The LEDs use approximately 20% brightness.

At startup, audio output is explicitly `UNKNOWN`; no output is assumed and both DualKey LEDs remain off. A direct left or right selection establishes the internal output state. A both-key toggle reverses a known internal state. If the first audio action after startup is the toggle shortcut, the result cannot be inferred, so the state remains unknown and both LEDs stay off until a direct selection.

The local mute tracker starts off and toggles whenever the Encoder mute command is sent, so the first press lights the red LED. Neither output nor mute state is read back from macOS. Operations performed outside this controller can therefore make the LEDs disagree with macOS; use a direct output key to resynchronize the output indicator, and treat the mute LED as a local operation indicator.

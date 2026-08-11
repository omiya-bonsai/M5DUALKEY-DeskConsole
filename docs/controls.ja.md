# 操作とLED表示

English: [controls.md](controls.md)

本プロジェクトでは、USB-Cコネクタ側をDualKeyの正面として扱います。正面から見た物理配置は、左からAngle、Encoder、DualKeyです。以下の左右操作とLED位置は、すべてこの向きから見た表記です。

## DualKey

| 操作 | HIDショートカット | 想定するRaycast動作 |
| --- | --- | --- |
| 左キー | `Ctrl + Cmd + 2` | Studio Displayを選択 |
| 右キー | `Ctrl + Cmd + 1` | ORA4を選択 |
| 左右同時押し | `Ctrl + Option + S` | ORA4／Studio Displayをトグル |

同時押しを最優先します。短い同時押し判定時間を設け、ほぼ同時の押下で先に単独キー処理が実行されることを防ぎます。

Raycastを使用する操作は、この3つだけです。

## Encoder

| 操作 | USB HID Consumer Control |
| --- | --- |
| 時計回り | Volume Up |
| 反時計回り | Volume Down |
| 押し込み | Mute / Unmute |

Encoder操作はUSB HID Consumer ControlとしてmacOSへ直接送信され、Raycastを経由しません。

## Angle

| 位置 | 動作 |
| --- | --- |
| 左 | 上方向へオートスクロール |
| 中央 | スクロール停止 |
| 右 | 下方向へオートスクロール |

AngleのスクロールはUSB HIDマウスホイールとしてmacOSへ直接送信され、Raycastを経由しません。

起動時にAngleの中央位置をキャリブレーションします。開始と停止で異なるしきい値を使うヒステリシスにより、中央付近の小さな揺れでスクロールが頻繁に再開することを防ぎます。開始しきい値の外側では、中央からの距離に応じてx^1.5の速度カーブで加速します。

起動中はAngleを物理的な中央位置に置いてください。

## LED状態表示

| 内部状態 | LED表示 |
| --- | --- |
| オーディオ出力不明 | DualKey左右とも消灯 |
| Studio Display選択 | DualKey左LEDが白 |
| ORA4選択 | DualKey右LEDが青 |
| ローカルのミュート状態がON | Encoder LEDが赤 |
| ローカルのミュート状態がOFF | Encoder LEDが消灯 |
| Angleの全状態 | Angle LEDが消灯 |

LED輝度は約20%です。

起動時のオーディオ出力は明示的に`UNKNOWN`とし、特定の出力先を仮定せずDualKey左右LEDを消灯します。左または右の直接選択後に内部状態が確定します。左右同時押しは、内部状態が既知の場合だけ状態を反転します。起動後の最初の操作がトグルだった場合、結果を推測できないため不明状態と消灯を維持し、直接選択後から表示します。

ローカルのミュート状態はOFFで起動し、EncoderからMuteコマンドを送るたびに反転するため、最初の押下で赤く点灯します。出力先もミュートもmacOSから読み戻していません。macOSや別デバイスから操作するとLEDと実状態がずれる可能性があります。出力表示は左または右キーの直接選択で再同期できます。ミュートLEDはローカル操作状態の目安として扱ってください。

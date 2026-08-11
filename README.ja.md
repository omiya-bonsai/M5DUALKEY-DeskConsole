# M5DUALKEY-DeskConsole

🇺🇸 **English:** [README.md](README.md)

<p align="center">
  <img src="assets/images/dualkey.jpg" width="700" alt="M5DUALKEY-DeskConsole">
</p>

M5Stack Chainシリーズを利用した、macOS向けUSB HIDデスクトップコントローラーです。

OS依存の処理をファームウェアへ組み込むのではなく、USB HIDでショートカットキーを送信し、Raycastなどのデスクトップツールへ処理を委譲することを目的としています。

---

## 特徴

- スピーカー出力切り替え
- スピーカー出力トグル
- 音量調整
- ミュート
- ハンズフリーオートスクロール
- Angleの自動センターキャリブレーション

---

## 使用ハードウェア

- M5Stack Chain DualKey (ESP32-S3)
- M5Stack Chain Encoder
- M5Stack Chain Angle

現在の接続レイアウト

```text
+-----------+-----------+-----------+
| DualKey   | Encoder   | Angle     |
+-----------+-----------+-----------+
```

---

## 操作一覧

| モジュール | 操作 | 動作 |
|------------|------|------|
| DualKey | 左キー | `Ctrl + Cmd + 1` を送信 |
| DualKey | 右キー | `Ctrl + Cmd + 2` を送信 |
| DualKey | 左右同時押し | `Ctrl + Option + S` を送信 |
| Encoder | 時計回り | 音量アップ |
| Encoder | 反時計回り | 音量ダウン |
| Encoder | 押し込み | ミュート切り替え |
| Angle | 左へ回す | 上方向へオートスクロール |
| Angle | 中央 | スクロール停止 |
| Angle | 右へ回す | 下方向へオートスクロール |

---

## システム構成

本ファームウェアは、スピーカー切り替えなどのOS依存処理を実装していません。

USB HID経由でショートカットキーを送信し、その後の処理をRaycastへ委譲しています。

```text
M5DUALKEY-DeskConsole
          │
          ▼
       USB HID
          │
          ▼
        macOS
          │
          ▼
       Raycast
          │
          ▼
 AppleScript / Shell Script
          │
          ▼
 スピーカー切り替え
```

この構成により、

- ファームウェアはシンプルに保てる
- macOS側の処理だけ自由に変更できる
- ESP32を書き換えなくても操作内容を変更できる

というメリットがあります。

---

## Raycastとの連携

現在はRaycastへ以下のショートカットキーを登録しています。

| ショートカット | 動作 |
|---------------|------|
| Ctrl + Cmd + 1 | ORA4へ切り替え |
| Ctrl + Cmd + 2 | Studio Displayへ切り替え |
| Ctrl + Option + S | オーディオ出力トグル |
| Consumer Control | 音量調整 |
| Consumer Control | ミュート |

スピーカー切り替えの実際の処理は、RaycastからAppleScriptやShell Scriptを実行しています。

同様の構成は、

- BetterTouchTool
- Keyboard Maestro
- Hammerspoon
- Karabiner-Elements

などにも応用できます。

---

## オートスクロール

Angleモジュールをスプリングセンター付きのスロットルとして利用しています。

- 左へ回すと上方向へスクロール
- 右へ回すと下方向へスクロール
- 中央へ戻すと停止
- 中央から離れるほど滑らかに加速します
- 起動時に自動でセンター位置をキャリブレーションします

---

## 開発環境

- Arduino IDE
- ESP32 Arduino Core (M5Stack)

使用ライブラリ

- M5Unified
- M5Chain
- USB HID

---

## 開発状況

実装済み

- ✅ USB HID Keyboard
- ✅ USB HID Consumer Control
- ✅ DualKey
- ✅ Encoder
- ✅ Angle Auto Scroll
- ✅ 自動キャリブレーション

今後の予定

- ⏳ RGB LED表示
- ⏳ BLE HID対応

---

## ライセンス

MIT License

詳細は [LICENSE](LICENSE) を参照してください。

---

## Maintainer

**omiya-bonsai**

https://github.com/omiya-bonsai

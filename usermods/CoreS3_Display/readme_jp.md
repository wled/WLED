# WLED on M5Stack CoreS3

[English](readme.md)

M5Stack CoreS3 上で WLED v17 系をネイティブ動作させ、<br>
**タッチディスプレイ / 内蔵マイク Audio Reactive / バッテリー表示 / 安全な電源OFF / LED制御** を1台にまとめるプロジェクトです。

> **Status:** CoreS3 実機検証済み実装<br>
> **Base:** WLED 17.0.0-devV5<br>
> **Target:** M5Stack CoreS3 (ESP32-S3 / 16MB Flash / 8MB Quad PSRAM)
>
> 本プロジェクトはコミュニティによる WLED の CoreS3 向け移植・拡張であり、WLED または M5Stack の公式ファームウェアではありません。

---

## Features

- WLED を M5Stack CoreS3 上でネイティブ実行
- 320 × 240 タッチディスプレイによるローカル操作
- WLED Web UI と CoreS3 UI の双方向同期
- LED Brightness / Effect / Speed / Intensity / Palette 操作
- C1 / C2 / C3 のマルチカラー編集
- Preset 呼び出し / SAVE / OVERWRITE / DELETE / BOOT PRESET
- 内蔵 ES7210 デュアルマイクを利用した Audio Reactive
- バッテリー残量表示
- Display Sleep / Wake
- Wi-Fi Offline / Recovery AP UX
- AXP2101 Power Key を使った Safe Shutdown
- 電源OFF前に LED BLACK frame を送信
- Safe Shutdown のキャンセル時は直前の LED 状態を復元
- ESP32-S3 / NeoPixelBus 向け RMT DMA1024 および LCD/GDMA runtime-rebuild 安定化
- ブラウザから現在の LCD 画面を BMP で取得

---

## Screenshots

以下は、CoreS3 本体の LCD を Browser Screenshot 機能で直接取得した実画面です。

### Main

<p align="center">
  <img src="screenshots/main.png" width="320">
</p>

MAIN 画面では、LED Power、Brightness、Effect、Color、Preset、およびネットワーク / バッテリー状態を確認できます。

### Effect

<table>
  <tr>
    <td><img src="screenshots/effect-solid.png" width="320"></td>
    <td><img src="screenshots/effect-rocktaves.png" width="320"></td>
  </tr>
  <tr>
    <td align="center">Standard Effect</td>
    <td align="center">Audio Reactive Effect</td>
  </tr>
</table>

Effect ごとに使用可能なパラメータだけを表示します。<br>
Audio Reactive Effect では、通常 Effect と異なるパラメータ構成にも追従します。

### Color

<table>
  <tr>
    <td><img src="screenshots/color-c1.png" width="320"></td>
    <td><img src="screenshots/color-c1-c2.png" width="320"></td>
    <td><img src="screenshots/color-unused.png" width="320"></td>
  </tr>
  <tr>
    <td align="center">C1 Edit</td>
    <td align="center">C1 / C2</td>
    <td align="center">Color Not Used</td>
  </tr>
</table>

C1 / C2 / C3 を個別に選択して編集できます。<br>
Effect が Color Slot を使用しない場合は `COLOR NOT USED` と表示します。

### Preset

<table>
  <tr>
    <td><img src="screenshots/preset.png" width="320"></td>
    <td><img src="screenshots/preset-manage.png" width="320"></td>
    <td><img src="screenshots/preset-save.png" width="320"></td>
  </tr>
  <tr>
    <td align="center">Preset</td>
    <td align="center">Preset Manage</td>
    <td align="center">Save New</td>
  </tr>
  <tr>
    <td><img src="screenshots/preset-delete.png" width="320"></td>
    <td><img src="screenshots/preset-boot.png" width="320"></td>
    <td></td>
  </tr>
  <tr>
    <td align="center">Delete Preset</td>
    <td align="center">Boot Preset</td>
    <td></td>
  </tr>
</table>

Preset の呼び出しだけでなく、SAVE NEW / OVERWRITE / DELETE / BOOT PRESET まで CoreS3 から操作できます。

### Browser Screenshot

CoreS3 の現在の LCD 表示は、ブラウザから次の URL で取得できます。

```text
http://<CoreS3-IP>/cores3/screenshot.bmp
```

例:

```text
http://192.168.1.100/cores3/screenshot.bmp
```

Screenshot は静止画です。<br>
URL を更新すると、その時点の 320 × 240 LCD 内容を再取得します。

BMP は CoreS3 の LCD から RGB565 で読み出し、24-bit BMP に変換して返します。<br>
大きな Screenshot バッファは通常時には常駐せず、要求時のみ PSRAM に確保します。

このドキュメントに掲載している PNG 画像も、Browser Screenshot で取得した BMP を PC 側で PNG に変換したものです。

---

## Tested Hardware

### Controller

- M5Stack CoreS3
- ESP32-S3 240 MHz
- 16 MB Flash
- 8 MB Quad PSRAM
- AXP2101 PMIC
- ES7210 audio codec / built-in microphones
- 320 × 240 touch display

### LED

実機確認では M5Stack の DIGITAL RGB LED STRIP を使用しています。

- SK6812
- RGB type
- 60 LEDs / m
- 2本連結で物理的には 120 LEDs
- WLED Bus Type: `WS281x`
- Color Order: `GRB`
- CoreS3 Port.C
- GPIO17

> **Important:** 120 LEDs を CoreS3 から直接高輝度で駆動する電源構成は、本プロジェクトでは安全性を保証していません。<br>
> LED 数や輝度が大きい場合は、LED 用の適切な外部 5V 電源と共通 GND を使用してください。

---

## Recommended Brightness

WLED 標準の初期 Brightness は変更していません。

CoreS3 から LED Strip を直接使用する場合、実機では Brightness 128 でも USB-C 給電中にバッテリー残量が低下する状況を確認しています。

そのため、CoreS3 での開始値としては **Brightness 64 前後**を推奨します。

```text
WLED default:           128
CoreS3 recommendation:  64
```

これは WLED 本体のデフォルト値を変更するものではありません。<br>
使用 LED 数、色、Effect、外部電源構成によって消費電力は大きく変化します。

---

## Build Environment

検証済みのビルド環境:

```text
WLED                  17.0.0-devV5
PlatformIO env        m5stack_cores3
Platform              pioarduino / platform-espressif32 55.03.39
Arduino Core           3.3.9
ESP-IDF libraries      5.5.4
NeoPixelBus            2.9.0+sha.76afe83
```

`platformio_override.ini` で CoreS3 用の環境を定義しています。

主な設定:

```ini
[platformio]
default_envs = m5stack_cores3

[env:m5stack_cores3]
extends = env:esp32s3dev_16MB_opi

board_build.arduino.memory_type = qio_qspi
board_build.flash_mode = qio
```

CoreS3 固有 Usermod:

```text
CoreS3_Power
CoreS3_Display
CoreS3_Audio
audioreactive
```

Audio Reactive 用:

```text
WLED_M5STACK_CORES3_AUDIO=1
UM_AUDIOREACTIVE_ENABLE
SR_DMTYPE=7
```

GPIO0 は ES7210 MCLK として使用するため、WLED の物理 Button から除外しています。

---


### 公開用 Build Example

CoreS3 用の PlatformIO 設定サンプルを次の場所に同梱しています。

```text
usermods/CoreS3_Display/platformio_override.ini.sample
```

このファイルを WLED リポジトリ直下へコピーし、次の名前に変更して使用します。

```text
platformio_override.ini
```

このサンプルには、CoreS3 Environment、Quad PSRAM 設定、CoreS3 Usermod、Audio Reactive 定義、および NeoPixelBus patch 用 pre-script が含まれています。

## Build

### 1. Requirements

- Visual Studio Code
- PlatformIO IDE
- Git
- USB-C cable

### 2. Open the WLED source tree

WLED リポジトリを VS Code で開きます。

`platformio_override.ini` がリポジトリ直下にあることを確認してください。存在しない場合は、`usermods/CoreS3_Display/platformio_override.ini.sample` をリポジトリ直下へコピーし、`platformio_override.ini` にリネームしてください。

### 3. Build

PlatformIO で次の Environment を Build します。

```text
m5stack_cores3
```

正常時は最後に次のように表示されます。

```text
Environment     Status
--------------  -------
m5stack_cores3  SUCCESS
```

### 4. Upload

PlatformIO から `m5stack_cores3` を Upload します。

シリアルモニタなどのプログラムが COM ポートを開いている場合は、Upload 前に閉じてください。


---

## NeoPixelBus / RMT DMA1024 + LCD/GDMA Patch

ESP32-S3 + NeoPixelBus の RMT 出力では、LED Count より後ろのピクセルが不定期に点灯する問題を実機で確認しました。

CoreS3 向けには次の安定化を適用しています。

```text
RMT DMA             enabled
mem_block_symbols   1024
```

パッチは `.pio/libdeps` のライブラリを手作業で変更する方式ではありません。

```text
pio-scripts/cores3_v17_neopixelbus_patch.py
```

が PlatformIO の pre-script として動作し、NeoPixelBus を新規取得した場合でも自動的に DMA1024 patch を適用します。

Build 時の例:

```text
[CoreS3 RMT DMA1024] applied ESP32-S3 DMA / 1024-symbol patch: ...
```

すでに適用済みの場合:

```text
[CoreS3 RMT DMA1024] patch already present: NeoEsp32RmtXMethod.h
```

同じ pre-script では、runtime の LED Bus 再構築時に使用する LCD/GDMA teardown 修正も適用します。<br>
最後の LCD mux bus を破棄する際に GDMA channel を stop / reset / disconnect / delete し、次回の Bus 初期化へ古い LCD peripheral ownership が残らないようにします。

LCD/GDMA patch 適用時の例:

```text
[CoreS3 LCD GDMA] applied full GDMA teardown production patch: ...
```

すでに適用済みの場合:

```text
[CoreS3 LCD GDMA] patch already present: NeoEsp32LcdXMethod.h
```

2つの patch はどちらも idempotent です。NeoPixelBus dependency を削除したクリーンな状態からの再 Build でも再適用性を確認済みです。

---

## Power Management

### DCDC3 Always-PWM

CoreS3 の予期しない完全電源OFF調査では、AXP2101 の `PWROFF_STATUS` で DCDC OVP を複数回確認しました。

CoreS3 Power Usermod では、DCDC3 を Always-PWM に設定する安定化策を使用しています。

```text
DCDC1: AUTO
DCDC3: ALWAYS_PWM
OVP protection: unchanged / enabled
```

この設定は長時間実機試験と回帰確認で安定化効果を確認しています。

> DCDC3 がハードウェア上の絶対的な根本原因だった、と断定しているわけではありません。<br>
> 本プロジェクトでは「実機で強く検証された安定化策」として扱っています。

### Safe Shutdown

CoreS3 の物理 Power Key 長押しでは、PMIC hard-off の前に LED Strip へ BLACK frame を送信します。

処理の概要:

```text
Power Key long press
        ↓
LED BLACK frame
        ↓
Strip suspend
        ↓
AXP2101 hard power-off
```

BLACK 送信後に Power Key を離して Shutdown をキャンセルした場合は、直前の LED 出力と Brightness を復元します。

---

## Audio Reactive

CoreS3 内蔵 ES7210 と内蔵マイクを使用します。

Audio Usermod が ES7210 を初期化し、Audio Reactive 側が I2S1 / PCM / FFT を担当します。

主な仕様:

```text
Codec          ES7210
I2S            I2S1
Sample Rate    16000 Hz
Format         Stereo / 16-bit
MCLK           GPIO0
DIN            GPIO14
```

起動時に Codec 初期化を少し遅延し、失敗時は複数回 Retry する構成です。

WLED Info から Audio の状態を確認できます。

---

## Touch UI

主な画面:

```text
MAIN
COLOR
EFFECT
PRESET
PRESET MANAGE
```

CoreS3 と WLED Web UI は双方向に同期します。

- CoreS3 で変更 → Web UI に反映
- Web UI で変更 → CoreS3 に反映

Touch の短押し / 長押し / ドラッグ操作もサポートしています。

---

## Preset Management

CoreS3 から次の操作ができます。

```text
Preset Call
SAVE NEW
OVERWRITE
DELETE
BOOT PRESET
```

Preset cache を使用し、WLED 側で保存された Preset と同期します。

---

## Network Recovery

Wi-Fi 接続が失われた場合、CoreS3 Display から Recovery AP を起動できる構成です。

Recovery AP 使用時は次へアクセスします。

```text
http://4.3.2.1
```

通常の WLED Web UI と CoreS3 のローカル操作を併用できます。

---

## Battery Status

CoreS3 の AXP2101 fuel gauge からバッテリー残量を取得し、MAIN 画面に表示します。

Battery status は定期更新され、常時 I2C polling し続けないよう間隔を設けています。

---

## Project Structure

CoreS3 対応の中心は次のファイルです。

```text
WLED/
├─ platformio_override.ini
├─ pio-scripts/
│  └─ cores3_v17_neopixelbus_patch.py
└─ usermods/
   ├─ CoreS3_Power/
   ├─ CoreS3_Display/
   ├─ CoreS3_Audio/
   └─ audioreactive/
```

### CoreS3_Power

担当:

- AXP2101
- AW9523B
- External 5V
- DCDC3 Always-PWM
- Power Key
- Safe Shutdown
- Power Health

### CoreS3_Display

担当:

- M5GFX
- Touch
- Local UI
- Battery display
- Wi-Fi status
- Preset UI
- Browser Screenshot

### CoreS3_Audio

担当:

- ES7210 probe / initialization
- Audio pins
- Audio health
- Audio Reactive handoff

### audioreactive

CoreS3 built-in microphone / I2S1 連携を追加しています。

---

## Current Limitations / Notes

- Local UI では Segment 管理を行いません<br>
  Segment 設定は WLED Web UI を使用します。
- 120 LED を CoreS3 から直接高輝度で給電する構成は推奨しません。
- WLED 標準 Brightness 128 は変更していません。
- CoreS3 では Brightness 64 前後からの使用を推奨します。
- Browser Screenshot は BMP の静止画です。ライブストリームではありません。
- DCDC OVP 保護は無効化していません。

---

## Upstream

This project is based on WLED:

https://github.com/wled/WLED

WLED itself remains the upstream project.<br>
Please also refer to the upstream repository for WLED documentation, supported LED types, API behavior, and licensing.

---

## Licensing

このリポジトリの WLED ソースは、upstream と同じ **EUPL v1.2** に従います。<br>
NeoPixelBus は **LGPL-3.0-or-later** のままです。CoreS3 の build-time patch script は PlatformIO が取得した NeoPixelBus ソースへ修正を適用しますが、upstream library のライセンスヘッダーは保持します。

完全なライセンス条件については、リポジトリの `LICENSE` と各 upstream project を参照してください。

---

## Acknowledgements

- WLED project and contributors
- M5Stack
- NeoPixelBus
- Audio Reactive / WLED usermod contributors

This project builds on the work of many open-source projects and contributors.

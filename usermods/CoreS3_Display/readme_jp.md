# WLED on M5Stack CoreS3

[English](readme.md)

M5Stack CoreS3 上で WLED v17 ファームウェアを直接動作させ、<br>
**タッチによるローカル操作 / 内蔵マイク Audio Reactive / バッテリー表示 / 物理 Power Key 連携 / LED制御** を1台にまとめるプロジェクトです。

> **Status:** CoreS3 実機検証済み実装<br>
> **Base:** WLED 17.0.0-devV5<br>
> **Target:** M5Stack CoreS3 (ESP32-S3 / 16MB Flash / 8MB Quad PSRAM)
>
> 本プロジェクトはコミュニティによる WLED の CoreS3 向け移植・拡張であり、WLED または M5Stack の公式ファームウェアではありません。

---

## Features

- WLED ファームウェアを M5Stack CoreS3 上で直接動作
- 320 × 240 タッチディスプレイによるローカル操作
- Web UI で変更した照明の状態をタッチ画面に表示し、タッチ操作でも同じ WLED の状態を更新
- LED Brightness / Effect / Speed / Intensity / Palette 操作
- C1 / C2 / C3 の個別カラー編集
- Preset 呼び出し / SAVE / OVERWRITE / DELETE / BOOT PRESET
- 内蔵 ES7210 デュアルマイクを利用した Audio Reactive
- バッテリー残量表示
- Display Sleep / Wake
- Wi-Fi Offline / Recovery AP UX
- AXP2101 物理 Power Key の処理
- 長押しおよびキャンセル時に WLED 標準の Power OFF / ON transition を使用
- PMIC hard-off 前に WLED が再度 ON になった場合も標準 transition で自然に復帰

---

## Screenshots

以下は、CoreS3 本体の LCD ユーザーインターフェースの実画面です。

### Main

<p align="center">
  <img src="screenshots/main.png" alt="CoreS3 メイン操作画面" width="320">
</p>

MAIN 画面では、LED Power、Brightness、Effect、Color、Preset、およびネットワーク / バッテリー状態を確認できます。

### Effect

<table>
  <tr>
    <td><img src="screenshots/effect-solid.png" alt="CoreS3 標準エフェクト画面" width="320"></td>
    <td><img src="screenshots/effect-rocktaves.png" alt="CoreS3 Audio Reactive エフェクト画面" width="320"></td>
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
    <td><img src="screenshots/color-c1.png" alt="CoreS3 C1 カラー編集画面" width="320"></td>
    <td><img src="screenshots/color-c1-c2.png" alt="CoreS3 C1・C2 カラー編集画面" width="320"></td>
    <td><img src="screenshots/color-unused.png" alt="CoreS3 Color Not Used 画面" width="320"></td>
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
    <td><img src="screenshots/preset.png" alt="CoreS3 Preset 選択画面" width="320"></td>
    <td><img src="screenshots/preset-manage.png" alt="CoreS3 Preset 管理画面" width="320"></td>
    <td><img src="screenshots/preset-save.png" alt="CoreS3 新規 Preset 保存画面" width="320"></td>
  </tr>
  <tr>
    <td align="center">Preset</td>
    <td align="center">Preset Manage</td>
    <td align="center">Save New</td>
  </tr>
  <tr>
    <td><img src="screenshots/preset-delete.png" alt="CoreS3 Preset 削除画面" width="320"></td>
    <td><img src="screenshots/preset-boot.png" alt="CoreS3 Boot Preset 設定画面" width="320"></td>
    <td></td>
  </tr>
  <tr>
    <td align="center">Delete Preset</td>
    <td align="center">Boot Preset</td>
    <td></td>
  </tr>
</table>

Preset の呼び出しだけでなく、SAVE NEW / OVERWRITE / DELETE / BOOT PRESET まで CoreS3 から操作できます。

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

実機確認では M5Stack の DIGITAL RGB LED STRIP を使用しています。M5Stack の[公式資料](https://docs.m5stack.com/en/unit/Unit_RGB_LED_Strip)では、白色専用チャンネルを持たない SK6812 RGB として紹介されています。

- SK6812 RGB
- 60 LEDs / m
- 2本連結で物理的には 120 LEDs
- WLED Bus Type: `WS281x`
- Color Order: `GRB`
- CoreS3 Port.C
- GPIO17

LED 用電源の容量、配線、共通 GND の接続については、[WLED の配線ガイド](https://kno.wled.ge/basics/getting-started/)を参照してください。接続する LED に合わせて電源と輝度を選んでください。輝度の値だけでは消費電流は決まりません。

---

## Build Environment

公開サンプルが継承するビルド環境:

```text
WLED                  17.0.0-devV5
PlatformIO env        m5stack_cores3
Platform              Tasmota 2026.05.50
Arduino Core          3.3.8.260506
ESP-IDF libraries     5.5.4.260407
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
```

WLED 標準の汎用 `audioreactive` Usermod も有効化し、CoreS3 内蔵マイクの ES7210 対応に使用します。

Audio Reactive 用:

```text
UM_AUDIOREACTIVE_ENABLE
SR_DMTYPE=10
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

このサンプルには、CoreS3 Environment、Quad PSRAM 設定、CoreS3 Usermod、および Audio Reactive 定義が含まれています。

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

## Power Management

### DCDC3 Always-PWM

CoreS3 の予期しない完全電源OFF調査では、AXP2101 の `PWROFF_STATUS` で DCDC OVP を複数回確認しました。

CoreS3 Power Usermod では、DCDC3 を Always-PWM に設定する安定化策を使用しています。

```text
DCDC1: AUTO
DCDC3: ALWAYS_PWM
OVP protection: kept active
```

この設定は長時間実機試験と回帰確認で安定化効果を確認しています。

> DCDC3 がハードウェア上の絶対的な根本原因だった、と断定しているわけではありません。<br>
> 本プロジェクトでは「実機で強く検証された安定化策」として扱っています。

### Power Key Handling

CoreS3 の物理 Power Key を長押しすると、AXP2101 が最終的な PMIC hard-off を行う前に、WLED 標準の Power OFF 処理を要求します。

CoreS3 usermod 側では LED 出力を直接強制せず、WLED 標準の Power transition を使用します。これにより Brightness 状態、transition の反転、その他の WLED Power 処理は WLED Core 側で管理されます。

処理の概要:

```text
Power Key long press
        ↓
WLED standard OFF transition
        ↓
LED output reaches OFF
        ↓
AXP2101 hard power-off
```

最終的な PMIC hard-off の前に Power Key を離した場合は、必要に応じて WLED 標準の Power ON 処理を要求します。

Power OFF transition の途中で Web UI から WLED を再度 ON にした場合も、CoreS3 固有の LED 出力強制を行わず、WLED 標準 transition が反転して実行中のエフェクトへ復帰します。

---

## Audio Reactive

CoreS3 内蔵 ES7210 と内蔵マイクを使用します。

主な仕様:

```text
Codec          ES7210
I2S            I2S0
Sample Rate    22050 Hz
Codec Format   24-bit I2S
I2S Input      Mono / WLED 32-bit container
MCLK           GPIO0
DIN            GPIO14
```

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

タッチ画面と Web UI は同じ WLED を操作します。どちらで変更した照明の状態も、もう一方の画面に反映されます。

Segment の設定には WLED Web UI を使用してください。

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

---

## Network Recovery

Wi-Fi 接続とアクセスポイントのどちらも利用できない場合、MAIN 画面に `Offline - Hold Recovery AP` と表示されます。このネットワーク状態の表示領域を1.5秒間長押しすると、一時的な Recovery AP が起動します。

Recovery AP のパスワードは `wled1234` です（ビルド時に `WLED_AP_PASS` を変更していない場合）。

画面に表示された名前のアクセスポイントへ接続し、次のアドレスを開いてください。

```text
http://4.3.2.1
```

Recovery AP を起動しても、保存済みの AP 接続情報や AP 動作設定は変更されません。

---

## Battery Status

MAIN 画面には、CoreS3 の電源管理 IC である AXP2101 が報告するバッテリー残量をパーセントで表示します。

---

## Project Structure

CoreS3 固有 Usermod は `CoreS3_Power` と `CoreS3_Display` です。
WLED 標準の汎用 `audioreactive` Usermod には、CoreS3 構成で使用する ES7210 source も含まれます。

```text
WLED/
├─ platformio_override.ini
└─ usermods/
   ├─ CoreS3_Power/
   ├─ CoreS3_Display/
   └─ audioreactive/
```

### CoreS3_Power

担当:

- AXP2101
- AW9523B
- External 5V
- DCDC3 Always-PWM
- Power Key
- WLED Power Transition
- 電源初期化と外部 5V の状態

### CoreS3_Display

担当:

- M5GFX
- Touch
- Local UI
- Battery display
- Wi-Fi status
- Preset UI

### audioreactive

audioreactive Usermod は、新しい ES7210 デジタルマイク種別により CoreS3 内蔵マイクに対応しています。

---

## Acknowledgements

- WLED project and contributors
- M5Stack
- NeoPixelBus
- Audio Reactive / WLED usermod contributors

This project builds on the work of many open-source projects and contributors.

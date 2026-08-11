#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "M5Unified.h"
#include "M5Chain.h"
#include "USBHIDMouse.h"
#include <math.h>

// ============================================================
// DualKey
// ============================================================

#define PIN_KEY1 0
#define PIN_KEY2 17

m5::Button_Class Key1;
m5::Button_Class Key2;

USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
USBHIDMouse Mouse;

constexpr uint32_t CHORD_WINDOW_MS = 80;

enum class PendingKey {
  NONE,
  LEFT,
  RIGHT
};

PendingKey pending = PendingKey::NONE;
uint32_t pendingSince = 0;

bool chordConsumed = false;
bool singleConsumed = false;


// ============================================================
// Chain
// Physical layout:
// DualKey -> Encoder -> Angle
// ============================================================

#define CHAIN_RXD_PIN GPIO_NUM_5
#define CHAIN_TXD_PIN GPIO_NUM_6

Chain M5Chain;

device_list_t *device_list = nullptr;
uint16_t device_count = 0;
uint8_t opr_status = 0;

uint8_t encoder_id = 0;
uint8_t angle_id = 0;


// ============================================================
// Encoder state
// ============================================================

int16_t lastEncoderValue = 0;
bool encoderValueInitialized = false;

bool lastEncoderButton = false;
uint32_t lastEncoderButtonChangeMs = 0;

constexpr uint32_t ENCODER_BUTTON_DEBOUNCE_MS = 40;


// ============================================================
// Angle auto scroll
// ============================================================

constexpr uint16_t ANGLE_MIN = 0;
constexpr uint16_t ANGLE_MAX = 4095;

// 起動時キャリブレーションで実測値に更新する
uint16_t angleCenter = 2048;

// キャリブレーション
constexpr uint16_t ANGLE_CALIBRATION_SAMPLES = 40;
constexpr uint32_t ANGLE_CALIBRATION_INTERVAL_MS = 10;

// ヒステリシス
//
// スクロール中は ±95 に戻るとSTOP
// STOP中は ±135 を超えると再開
constexpr uint16_t ANGLE_STOP_OFFSET = 95;
constexpr uint16_t ANGLE_START_OFFSET = 135;

// Angle読み取り周期
constexpr uint32_t ANGLE_READ_INTERVAL_MS = 20;

// スクロール速度
constexpr uint32_t SCROLL_SLOWEST_INTERVAL_MS = 220;
constexpr uint32_t SCROLL_FASTEST_INTERVAL_MS = 25;

// デッドゾーンを抜けた直後から実用速度を確保
constexpr float SCROLL_BASE_SPEED = 0.35f;

// macOS wheel direction
constexpr int8_t SCROLL_UP_STEP = -1;
constexpr int8_t SCROLL_DOWN_STEP = 1;

uint16_t angleValue = 2048;
uint32_t lastAngleReadMs = 0;
uint32_t lastScrollMs = 0;

enum class ScrollState {
  STOPPED,
  UP,
  DOWN
};

ScrollState scrollState = ScrollState::STOPPED;


// ============================================================
// ANGLEのキャリブレーション
// スクロールのストップを強固にする
// ============================================================

void calibrateAngleCenter() {
  if (angle_id == 0) {
    return;
  }

  uint32_t sum = 0;

  for (uint16_t i = 0; i < ANGLE_CALIBRATION_SAMPLES; ++i) {
    uint16_t value = 0;

    M5Chain.getAngle12BitAdc(
      angle_id,
      &value);

    sum += value;
    delay(ANGLE_CALIBRATION_INTERVAL_MS);
  }

  angleCenter =
    (uint16_t)(sum / ANGLE_CALIBRATION_SAMPLES);

  angleValue = angleCenter;
  scrollState = ScrollState::STOPPED;
  lastScrollMs = millis();
}


// ============================================================
// HID: Speaker switching
// ============================================================

void sendOra4() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('1');
  delay(20);
  Keyboard.releaseAll();
}

void sendStudioDisplay() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('2');
  delay(20);
  Keyboard.releaseAll();
}

void sendAudioToggle() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('s');
  delay(20);
  Keyboard.releaseAll();
}


// ============================================================
// HID: Audio volume
// ============================================================

void volumeUp() {
  ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
  delay(5);
  ConsumerControl.release();
}

void volumeDown() {
  ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
  delay(5);
  ConsumerControl.release();
}

void toggleMute() {
  ConsumerControl.press(CONSUMER_CONTROL_MUTE);
  delay(5);
  ConsumerControl.release();
}


// ============================================================
// Chain initialization
// ============================================================

bool initChainDevices() {
  M5Chain.begin(
    &Serial2,
    115200,
    CHAIN_RXD_PIN,
    CHAIN_TXD_PIN);

  const uint32_t timeout = millis() + 3000;

  while (!M5Chain.isDeviceConnected()) {
    if ((int32_t)(millis() - timeout) >= 0) {
      return false;
    }
    delay(50);
  }

  M5Chain.getDeviceNum(&device_count);

  if (device_count == 0) {
    return false;
  }

  device_list =
    (device_list_t *)malloc(sizeof(device_list_t));

  if (!device_list) {
    return false;
  }

  device_list->count = device_count;

  device_list->devices =
    (device_info_t *)malloc(
      sizeof(device_info_t) * device_count);

  if (!device_list->devices) {
    free(device_list);
    device_list = nullptr;
    return false;
  }

  M5Chain.getDeviceList(device_list);

  // Chain上を走査してEncoder / Angleを自動探索
  for (uint16_t i = 0; i < device_count; ++i) {
    const uint8_t id = i + 1;
    const uint16_t type =
      device_list->devices[i].device_type;

    if (type == CHAIN_ENCODER_TYPE_CODE) {
      encoder_id = id;
    }

    if (type == CHAIN_ANGLE_TYPE_CODE) {
      angle_id = id;
    }
  }

  if (encoder_id != 0) {
    M5Chain.setEncoderABDirect(
      encoder_id,
      ENCODER_AB,
      &opr_status);

    M5Chain.setEncoderButtonTriggerInterval(
      encoder_id,
      BUTTON_DOUBLE_CLICK_TIME_500MS,
      BUTTON_LONG_PRESS_TIME_5S,
      &opr_status);
  }

  return true;
}


// ============================================================
// Angle processing
// ============================================================

void updateAngle() {
  if (angle_id == 0) {
    return;
  }

  const uint32_t now = millis();

  // ----------------------------------------------------------
  // Read Angle
  // ----------------------------------------------------------

  if (now - lastAngleReadMs >= ANGLE_READ_INTERVAL_MS) {
    lastAngleReadMs = now;

    uint16_t value = angleCenter;

    M5Chain.getAngle12BitAdc(
      angle_id,
      &value);

    angleValue = value;
  }

  // ----------------------------------------------------------
  // Neutral zone + hysteresis
  // ----------------------------------------------------------

  const int32_t center =
    (int32_t)angleCenter;

  const int32_t stopLow =
    center - ANGLE_STOP_OFFSET;

  const int32_t stopHigh =
    center + ANGLE_STOP_OFFSET;

  const int32_t startLow =
    center - ANGLE_START_OFFSET;

  const int32_t startHigh =
    center + ANGLE_START_OFFSET;

  switch (scrollState) {

    case ScrollState::STOPPED:

      if ((int32_t)angleValue <= startLow) {
        scrollState = ScrollState::UP;
      } else if ((int32_t)angleValue >= startHigh) {
        scrollState = ScrollState::DOWN;
      } else {
        return;
      }

      break;

    case ScrollState::UP:

      // 中央へ戻ってきたら停止
      if ((int32_t)angleValue >= stopLow) {
        scrollState = ScrollState::STOPPED;
        lastScrollMs = now;
        return;
      }

      break;

    case ScrollState::DOWN:

      // 中央へ戻ってきたら停止
      if ((int32_t)angleValue <= stopHigh) {
        scrollState = ScrollState::STOPPED;
        lastScrollMs = now;
        return;
      }

      break;
  }

  // ----------------------------------------------------------
  // START地点を0.0、物理端を1.0として正規化
  // ----------------------------------------------------------

  float normalizedDistance = 0.0f;

  if (scrollState == ScrollState::UP) {

    if ((int32_t)angleValue < startLow) {

      const float distance =
        (float)(startLow - (int32_t)angleValue);

      const float usableRange =
        (float)(startLow - (int32_t)ANGLE_MIN);

      if (usableRange > 0.0f) {
        normalizedDistance =
          distance / usableRange;
      }
    }
  }

  else if (scrollState == ScrollState::DOWN) {

    if ((int32_t)angleValue > startHigh) {

      const float distance =
        (float)((int32_t)angleValue - startHigh);

      const float usableRange =
        (float)((int32_t)ANGLE_MAX - startHigh);

      if (usableRange > 0.0f) {
        normalizedDistance =
          distance / usableRange;
      }
    }
  }

  if (normalizedDistance < 0.0f) {
    normalizedDistance = 0.0f;
  }

  if (normalizedDistance > 1.0f) {
    normalizedDistance = 1.0f;
  }

  // ----------------------------------------------------------
  // x^1.5 speed curve
  //
  // STOPを抜けた直後から実用的な低速を確保し、
  // 端へ近づくほど加速する
  // ----------------------------------------------------------

  const float curve =
    powf(normalizedDistance, 1.5f);

  const float speed =
    SCROLL_BASE_SPEED + (1.0f - SCROLL_BASE_SPEED) * curve;

  const uint32_t interval =
    SCROLL_SLOWEST_INTERVAL_MS - (uint32_t)(speed * (SCROLL_SLOWEST_INTERVAL_MS - SCROLL_FASTEST_INTERVAL_MS));

  // ----------------------------------------------------------
  // Send wheel event
  // ----------------------------------------------------------

  if (now - lastScrollMs >= interval) {
    lastScrollMs = now;

    if (scrollState == ScrollState::UP) {
      Mouse.move(
        0,
        0,
        SCROLL_UP_STEP);
    } else if (scrollState == ScrollState::DOWN) {
      Mouse.move(
        0,
        0,
        SCROLL_DOWN_STEP);
    }
  }
}


// ============================================================
// setup
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_KEY1, INPUT);
  pinMode(PIN_KEY2, INPUT);

  Keyboard.begin();
  ConsumerControl.begin();
  Mouse.begin();
  USB.begin();

  initChainDevices();

  // 起動時はAngleを物理的な中央位置に置いておく
  calibrateAngleCenter();
}


// ============================================================
// DualKey processing
// ============================================================

void updateDualKey() {
  const uint32_t now = millis();

  Key1.setRawState(now, !digitalRead(PIN_KEY1));
  Key2.setRawState(now, !digitalRead(PIN_KEY2));

  const bool leftPressed = Key1.isPressed();
  const bool rightPressed = Key2.isPressed();

  // 両押し最優先
  if (leftPressed && rightPressed) {
    if (!chordConsumed) {
      pending = PendingKey::NONE;

      sendAudioToggle();

      chordConsumed = true;
      singleConsumed = false;
    }
    return;
  }

  if (chordConsumed) {
    if (!leftPressed && !rightPressed) {
      chordConsumed = false;
    }
    return;
  }

  if (singleConsumed) {
    if (!leftPressed && !rightPressed) {
      singleConsumed = false;
    }
    return;
  }

  if (pending == PendingKey::NONE) {
    if (leftPressed && !rightPressed) {
      pending = PendingKey::LEFT;
      pendingSince = now;
    } else if (rightPressed && !leftPressed) {
      pending = PendingKey::RIGHT;
      pendingSince = now;
    }
  }

  if (pending != PendingKey::NONE && now - pendingSince >= CHORD_WINDOW_MS) {

    if (pending == PendingKey::LEFT && leftPressed) {
      sendStudioDisplay();
      singleConsumed = true;
    } else if (pending == PendingKey::RIGHT && rightPressed) {
      sendOra4();
      singleConsumed = true;
    }

    pending = PendingKey::NONE;
  }

  if (pending == PendingKey::LEFT && Key1.wasReleased()) {

    sendStudioDisplay();
    pending = PendingKey::NONE;
    singleConsumed = true;
  }

  if (pending == PendingKey::RIGHT && Key2.wasReleased()) {

    sendOra4();
    pending = PendingKey::NONE;
    singleConsumed = true;
  }
}


// ============================================================
// Encoder processing
// ============================================================

void updateEncoder() {
  if (encoder_id == 0) {
    return;
  }

  // ----------------------------------------------------------
  // Rotation
  // ----------------------------------------------------------

  int16_t currentValue = 0;

  M5Chain.getEncoderValue(
    encoder_id,
    &currentValue);

  if (!encoderValueInitialized) {
    lastEncoderValue = currentValue;
    encoderValueInitialized = true;
  } else {
    const int16_t delta =
      currentValue - lastEncoderValue;

    if (delta > 0) {
      volumeUp();
      lastEncoderValue = currentValue;
    } else if (delta < 0) {
      volumeDown();
      lastEncoderValue = currentValue;
    }
  }

  // ----------------------------------------------------------
  // Push button
  // ----------------------------------------------------------

  uint8_t buttonStatus = 0;

  M5Chain.getEncoderButtonStatus(
    encoder_id,
    &buttonStatus);

  const bool pressed =
    (buttonStatus != 0);

  const uint32_t now =
    millis();

  if (pressed != lastEncoderButton && (now - lastEncoderButtonChangeMs) >= ENCODER_BUTTON_DEBOUNCE_MS) {

    lastEncoderButtonChangeMs = now;

    if (pressed) {
      toggleMute();
    }

    lastEncoderButton = pressed;
  }
}


// ============================================================
// loop
// ============================================================

void loop() {
  updateDualKey();
  updateEncoder();
  updateAngle();

  delay(2);
}
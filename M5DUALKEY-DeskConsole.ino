#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "M5Unified.h"
#include "M5Chain.h"
#include "USBHIDMouse.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ============================================================
// DualKey
// ============================================================

#define PIN_KEY1 0
#define PIN_KEY2 17

m5::Button_Class Key1;
m5::Button_Class Key2;

// Front orientation:
// USB-C is on the rear side.
//
// Physical DualKey mapping confirmed on hardware:
// LEFT  = GPIO17 = ORA4
// RIGHT = GPIO0  = Studio Display

#define PIN_KEY_LEFT 17
#define PIN_KEY_RIGHT 0

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
// LED state
// ============================================================

constexpr uint8_t DUALKEY_LED_POWER_PIN = 40;
constexpr uint8_t DUALKEY_LED_SIGNAL_PIN = 21;
constexpr uint8_t DUALKEY_LED_COUNT = 2;

// Front orientation: USB-C is on the rear side.
//
// Physical layout:
// DualKey -> Encoder -> Angle
//
// DualKey mapping:
// LEFT  = ORA4           = Red
// RIGHT = Studio Display = Yellow
constexpr uint8_t DUALKEY_LEFT_LED_INDEX = 0;
constexpr uint8_t DUALKEY_RIGHT_LED_INDEX = 1;

// LED animation update interval
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 30;

// Base colors

constexpr uint8_t ORA4_R = 255;
constexpr uint8_t ORA4_G = 40;
constexpr uint8_t ORA4_B = 40;

constexpr uint8_t STUDIO_R = 255;
constexpr uint8_t STUDIO_G = 220;
constexpr uint8_t STUDIO_B = 0;

constexpr uint8_t MUTE_R = 170;
constexpr uint8_t MUTE_G = 40;
constexpr uint8_t MUTE_B = 255;

constexpr uint8_t ANGLE_R = 40;
constexpr uint8_t ANGLE_G = 140;
constexpr uint8_t ANGLE_B = 255;

// Brightness range.
//
// Audio output LEDs:
//   35% -> 80%
//
// Mute LED:
//   45% -> 85%
constexpr float AUDIO_LED_MIN_LEVEL = 0.35f;
constexpr float AUDIO_LED_MAX_LEVEL = 0.80f;

constexpr float MUTE_LED_MIN_LEVEL = 0.45f;
constexpr float MUTE_LED_MAX_LEVEL = 0.85f;

constexpr float ANGLE_LED_MIN_LEVEL = 0.30f;
constexpr float ANGLE_LED_ACTIVITY_EXPONENT = 0.7f;
constexpr uint32_t ANGLE_LED_FADE_MS = 150;

// How strongly the irregular fluctuation affects breathing.
// 0.0 = pure regular breathing
// 1.0 = much more irregular
constexpr float LED_FLUCTUATION_STRENGTH = 0.18f;

// Chain LED master brightness.
// Leave high enough so our RGB scaling can control intensity.
constexpr uint8_t CHAIN_LED_BRIGHTNESS = 255;

Adafruit_NeoPixel DualKeyLeds(
  DUALKEY_LED_COUNT,
  DUALKEY_LED_SIGNAL_PIN,
  NEO_GRB + NEO_KHZ800);

enum class AudioOutput {
  UNKNOWN,
  STUDIO_DISPLAY,
  ORA4
};

AudioOutput currentAudioOutput = AudioOutput::UNKNOWN;

bool muted = false;
bool ledsDirty = true;

uint32_t lastLedUpdateMs = 0;

uint8_t lastEncoderLedRgb[3] = { 0, 0, 0 };
uint8_t lastAngleLedRgb[3] = { 0, 0, 0 };
bool encoderLedRgbInitialized = false;
bool angleLedRgbInitialized = false;


// ============================================================
// Boot LED animation
// ============================================================

// Timing
constexpr float BOOT_SEQUENCE_LEVEL = 0.55f;
constexpr uint32_t BOOT_STEP_MS = 140;
constexpr uint32_t BOOT_READY_HOLD_MS = 250;

// Fade-out
constexpr uint8_t BOOT_FADE_STEPS = 18;
constexpr uint32_t BOOT_FADE_STEP_MS = 22;


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

float angleActivityLevel = 0.0f;
float angleLedLevel = 0.0f;
float angleFadeStartLevel = 0.0f;
uint32_t angleFadeStartMs = 0;
bool angleLedFading = false;


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
  angleActivityLevel = 0.0f;
  angleLedLevel = 0.0f;
  angleLedFading = false;
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
// LED control
// ============================================================

void setAudioOutputState(AudioOutput output) {
  currentAudioOutput = output;
  ledsDirty = true;
}

void toggleAudioOutputState() {
  if (currentAudioOutput == AudioOutput::STUDIO_DISPLAY) {
    setAudioOutputState(AudioOutput::ORA4);
  } else if (currentAudioOutput == AudioOutput::ORA4) {
    setAudioOutputState(AudioOutput::STUDIO_DISPLAY);
  }
}

void toggleMuteState() {
  muted = !muted;
  ledsDirty = true;
}

bool rgbNeedsUpdate(
  uint8_t r,
  uint8_t g,
  uint8_t b,
  const uint8_t lastRgb[3],
  bool initialized) {
  if (!initialized) {
    return true;
  }

  if (r == 0 && g == 0 && b == 0 && (lastRgb[0] != 0 || lastRgb[1] != 0 || lastRgb[2] != 0)) {
    return true;
  }

  const int rDifference =
    abs((int)r - (int)lastRgb[0]);
  const int gDifference =
    abs((int)g - (int)lastRgb[1]);
  const int bDifference =
    abs((int)b - (int)lastRgb[2]);

  return rDifference > 1 || gDifference > 1 || bDifference > 1;
}

void setChainRgb(
  uint8_t deviceId,
  uint8_t r,
  uint8_t g,
  uint8_t b,
  uint8_t lastRgb[3],
  bool &initialized) {
  if (deviceId == 0 || !rgbNeedsUpdate(r, g, b, lastRgb, initialized)) {
    return;
  }

  uint8_t rgb[3] = {
    r,
    g,
    b
  };

  M5Chain.setRGBValue(
    deviceId,
    0,
    1,
    rgb,
    sizeof(rgb),
    &opr_status);

  lastRgb[0] = r;
  lastRgb[1] = g;
  lastRgb[2] = b;
  initialized = true;
}

void setEncoderRgb(
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  setChainRgb(
    encoder_id,
    r,
    g,
    b,
    lastEncoderLedRgb,
    encoderLedRgbInitialized);
}

void setAngleRgb(
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  setChainRgb(
    angle_id,
    r,
    g,
    b,
    lastAngleLedRgb,
    angleLedRgbInitialized);
}

uint8_t scaleBootChannel(
  uint8_t channel,
  float level) {
  return (uint8_t)(channel * level);
}


void playBootLedAnimation() {
  // ----------------------------------------------------------
  // Start completely dark
  // ----------------------------------------------------------

  DualKeyLeds.clear();
  DualKeyLeds.show();

  setEncoderRgb(0, 0, 0);
  setAngleRgb(0, 0, 0);

  delay(80);

  // ----------------------------------------------------------
  // 1. Front-left:
  //    DualKey LEFT = ORA4
  // ----------------------------------------------------------

  DualKeyLeds.setPixelColor(
    DUALKEY_LEFT_LED_INDEX,
    DualKeyLeds.Color(
      scaleBootChannel(ORA4_R, BOOT_SEQUENCE_LEVEL),
      scaleBootChannel(ORA4_G, BOOT_SEQUENCE_LEVEL),
      scaleBootChannel(ORA4_B, BOOT_SEQUENCE_LEVEL)));

  DualKeyLeds.show();
  delay(BOOT_STEP_MS);

  // ----------------------------------------------------------
  // 2. DualKey RIGHT = Studio Display
  // ----------------------------------------------------------

  DualKeyLeds.setPixelColor(
    DUALKEY_RIGHT_LED_INDEX,
    DualKeyLeds.Color(
      scaleBootChannel(STUDIO_R, BOOT_SEQUENCE_LEVEL),
      scaleBootChannel(STUDIO_G, BOOT_SEQUENCE_LEVEL),
      scaleBootChannel(STUDIO_B, BOOT_SEQUENCE_LEVEL)));

  DualKeyLeds.show();
  delay(BOOT_STEP_MS);

  // ----------------------------------------------------------
  // 3. Encoder
  // ----------------------------------------------------------

  setEncoderRgb(
    scaleBootChannel(MUTE_R, BOOT_SEQUENCE_LEVEL),
    scaleBootChannel(MUTE_G, BOOT_SEQUENCE_LEVEL),
    scaleBootChannel(MUTE_B, BOOT_SEQUENCE_LEVEL));

  delay(BOOT_STEP_MS);

  // ----------------------------------------------------------
  // 4. Angle
  // ----------------------------------------------------------

  setAngleRgb(
    scaleBootChannel(ANGLE_R, BOOT_SEQUENCE_LEVEL),
    scaleBootChannel(ANGLE_G, BOOT_SEQUENCE_LEVEL),
    scaleBootChannel(ANGLE_B, BOOT_SEQUENCE_LEVEL));

  delay(BOOT_STEP_MS);

  // ----------------------------------------------------------
  // READY:
  // Bring all four status LEDs near full brightness once.
  // ----------------------------------------------------------

  DualKeyLeds.setPixelColor(
    DUALKEY_LEFT_LED_INDEX,
    DualKeyLeds.Color(
      ORA4_R,
      ORA4_G,
      ORA4_B));

  DualKeyLeds.setPixelColor(
    DUALKEY_RIGHT_LED_INDEX,
    DualKeyLeds.Color(
      STUDIO_R,
      STUDIO_G,
      STUDIO_B));

  DualKeyLeds.show();

  setEncoderRgb(
    MUTE_R,
    MUTE_G,
    MUTE_B);

  setAngleRgb(
    ANGLE_R,
    ANGLE_G,
    ANGLE_B);

  delay(BOOT_READY_HOLD_MS);

  // ----------------------------------------------------------
  // Smooth fade-out
  // ----------------------------------------------------------

  for (int step = BOOT_FADE_STEPS;
       step >= 0;
       --step) {

    const float level =
      (float)step / (float)BOOT_FADE_STEPS;

    // Slight gamma curve makes the fade look natural.
    const float corrected =
      powf(level, 1.8f);

    DualKeyLeds.setPixelColor(
      DUALKEY_LEFT_LED_INDEX,
      DualKeyLeds.Color(
        scaleBootChannel(ORA4_R, corrected),
        scaleBootChannel(ORA4_G, corrected),
        scaleBootChannel(ORA4_B, corrected)));

    DualKeyLeds.setPixelColor(
      DUALKEY_RIGHT_LED_INDEX,
      DualKeyLeds.Color(
        scaleBootChannel(STUDIO_R, corrected),
        scaleBootChannel(STUDIO_G, corrected),
        scaleBootChannel(STUDIO_B, corrected)));

    DualKeyLeds.show();

    setEncoderRgb(
      scaleBootChannel(MUTE_R, corrected),
      scaleBootChannel(MUTE_G, corrected),
      scaleBootChannel(MUTE_B, corrected));

    setAngleRgb(
      scaleBootChannel(ANGLE_R, corrected),
      scaleBootChannel(ANGLE_G, corrected),
      scaleBootChannel(ANGLE_B, corrected));

    delay(BOOT_FADE_STEP_MS);
  }

  // ----------------------------------------------------------
  // End boot animation completely dark.
  //
  // Audio output is UNKNOWN at boot, so we must not pretend
  // ORA4 or Studio Display is selected.
  // ----------------------------------------------------------

  DualKeyLeds.clear();
  DualKeyLeds.show();

  setEncoderRgb(0, 0, 0);
  setAngleRgb(0, 0, 0);

  angleLedLevel = 0.0f;
  angleLedFading = false;

  ledsDirty = true;
}


// ------------------------------------------------------------
// Gamma correction
// ------------------------------------------------------------
//
// LED brightness is not perceived linearly by human vision.
// This makes the breathing motion look smoother.
//
float gammaCorrect(float x) {
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;

  return powf(x, 2.2f);
}


// ------------------------------------------------------------
// 1/f-like breathing waveform
// ------------------------------------------------------------
//
// Not strict mathematical 1/f noise.
//
// Instead:
//   - main slow breathing wave
//   - two slower low-amplitude waves
//
// Their periods are intentionally different so the pattern
// does not repeat obviously.
//
float getLedBreathingLevel(uint32_t nowMs) {
  const float t = nowMs / 1000.0f;

  // Main breathing: about 3.2 sec
  const float mainWave =
    0.5f + 0.5f * sinf(2.0f * PI * t / 3.2f);

  // Slow fluctuation: about 7.1 sec
  const float slowWave1 =
    sinf(
      2.0f * PI * t / 7.1f + 0.8f);

  // Even slower fluctuation: about 13.7 sec
  const float slowWave2 =
    sinf(
      2.0f * PI * t / 13.7f + 2.1f);

  const float fluctuation =
    LED_FLUCTUATION_STRENGTH * (0.65f * slowWave1 + 0.35f * slowWave2);

  float level =
    mainWave + fluctuation;

  if (level < 0.0f) level = 0.0f;
  if (level > 1.0f) level = 1.0f;

  return level;
}


// ------------------------------------------------------------
// Map breathing waveform into visible brightness range
// ------------------------------------------------------------

float getAudioLedLevel(uint32_t nowMs) {
  const float wave =
    getLedBreathingLevel(nowMs);

  return AUDIO_LED_MIN_LEVEL + (AUDIO_LED_MAX_LEVEL - AUDIO_LED_MIN_LEVEL) * wave;
}

float getMuteLedLevel(uint32_t nowMs) {
  const float wave =
    getLedBreathingLevel(nowMs);

  return MUTE_LED_MIN_LEVEL + (MUTE_LED_MAX_LEVEL - MUTE_LED_MIN_LEVEL) * wave;
}


// ------------------------------------------------------------
// Scale RGB color by perceptual brightness
// ------------------------------------------------------------

void scaleRgb(
  uint8_t baseR,
  uint8_t baseG,
  uint8_t baseB,
  float level,
  uint8_t &outR,
  uint8_t &outG,
  uint8_t &outB) {
  const float corrected =
    gammaCorrect(level);

  outR =
    (uint8_t)(baseR * corrected);

  outG =
    (uint8_t)(baseG * corrected);

  outB =
    (uint8_t)(baseB * corrected);
}


// ------------------------------------------------------------
// DualKey LEDs
// ------------------------------------------------------------

void updateDualKeyLeds(uint32_t nowMs) {
  DualKeyLeds.clear();

  if (currentAudioOutput == AudioOutput::UNKNOWN) {
    DualKeyLeds.show();
    return;
  }

  const float level =
    getAudioLedLevel(nowMs);

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  if (currentAudioOutput == AudioOutput::ORA4) {
    scaleRgb(
      ORA4_R,
      ORA4_G,
      ORA4_B,
      level,
      r,
      g,
      b);

    DualKeyLeds.setPixelColor(
      DUALKEY_LEFT_LED_INDEX,
      DualKeyLeds.Color(r, g, b));
  } else if (currentAudioOutput == AudioOutput::STUDIO_DISPLAY) {
    scaleRgb(
      STUDIO_R,
      STUDIO_G,
      STUDIO_B,
      level,
      r,
      g,
      b);

    DualKeyLeds.setPixelColor(
      DUALKEY_RIGHT_LED_INDEX,
      DualKeyLeds.Color(r, g, b));
  }

  DualKeyLeds.show();
}


// ------------------------------------------------------------
// Encoder LED
// ------------------------------------------------------------

void updateEncoderLed(uint32_t nowMs) {
  if (encoder_id == 0) {
    return;
  }

  uint8_t encoderRgb[3] = {
    0,
    0,
    0
  };

  if (muted) {
    const float level =
      getMuteLedLevel(nowMs);

    scaleRgb(
      MUTE_R,
      MUTE_G,
      MUTE_B,
      level,
      encoderRgb[0],
      encoderRgb[1],
      encoderRgb[2]);
  }

  setEncoderRgb(
    encoderRgb[0],
    encoderRgb[1],
    encoderRgb[2]);
}


// ------------------------------------------------------------
// Angle LED
// ------------------------------------------------------------

void updateAngleLed(uint32_t nowMs) {
  if (angle_id == 0) {
    return;
  }

  float targetLevel = 0.0f;

  if (scrollState != ScrollState::STOPPED) {
    angleLedFading = false;

    const float activityCurve =
      powf(
        angleActivityLevel,
        ANGLE_LED_ACTIVITY_EXPONENT);

    targetLevel =
      ANGLE_LED_MIN_LEVEL + (1.0f - ANGLE_LED_MIN_LEVEL) * activityCurve;
  } else if (angleLedLevel > 0.0f) {
    if (!angleLedFading) {
      angleLedFading = true;
      angleFadeStartMs = nowMs;
      angleFadeStartLevel = angleLedLevel;
    }

    const uint32_t fadeElapsed =
      nowMs - angleFadeStartMs;

    if (fadeElapsed < ANGLE_LED_FADE_MS) {
      const float fadeProgress =
        (float)fadeElapsed / (float)ANGLE_LED_FADE_MS;

      targetLevel =
        angleFadeStartLevel * (1.0f - fadeProgress);
    } else {
      angleLedFading = false;
    }
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  scaleRgb(
    ANGLE_R,
    ANGLE_G,
    ANGLE_B,
    targetLevel,
    r,
    g,
    b);

  setAngleRgb(r, g, b);
  angleLedLevel = targetLevel;
}


// ------------------------------------------------------------
// Initialize local DualKey LEDs
// ------------------------------------------------------------

void initLeds() {
  pinMode(
    DUALKEY_LED_POWER_PIN,
    OUTPUT);

  digitalWrite(
    DUALKEY_LED_POWER_PIN,
    HIGH);

  DualKeyLeds.begin();

  // Full master brightness.
  // Actual brightness is controlled by RGB scaling.
  DualKeyLeds.setBrightness(255);

  DualKeyLeds.clear();
  DualKeyLeds.show();
}


// ------------------------------------------------------------
// Initialize Chain LEDs
// ------------------------------------------------------------

void initChainLeds() {
  uint8_t off[3] = {
    0,
    0,
    0
  };

  if (encoder_id != 0) {
    M5Chain.setRGBLight(
      encoder_id,
      CHAIN_LED_BRIGHTNESS,
      &opr_status);

    M5Chain.setRGBValue(
      encoder_id,
      0,
      1,
      off,
      sizeof(off),
      &opr_status);
  }

  if (angle_id != 0) {
    M5Chain.setRGBLight(
      angle_id,
      CHAIN_LED_BRIGHTNESS,
      &opr_status);

    M5Chain.setRGBValue(
      angle_id,
      0,
      1,
      off,
      sizeof(off),
      &opr_status);
  }
}


// ------------------------------------------------------------
// Main LED updater
// ------------------------------------------------------------

void updateLeds() {
  const uint32_t now =
    millis();

  // Update immediately after state changes,
  // otherwise update animation at fixed interval.
  if (!ledsDirty && now - lastLedUpdateMs < LED_UPDATE_INTERVAL_MS) {
    return;
  }

  lastLedUpdateMs = now;

  updateDualKeyLeds(now);
  updateEncoderLed(now);
  updateAngleLed(now);

  ledsDirty = false;
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
        angleActivityLevel = 0.0f;
        return;
      }

      break;

    case ScrollState::UP:

      // 中央へ戻ってきたら停止
      if ((int32_t)angleValue >= stopLow) {
        scrollState = ScrollState::STOPPED;
        angleActivityLevel = 0.0f;
        lastScrollMs = now;
        return;
      }

      break;

    case ScrollState::DOWN:

      // 中央へ戻ってきたら停止
      if ((int32_t)angleValue <= stopHigh) {
        scrollState = ScrollState::STOPPED;
        angleActivityLevel = 0.0f;
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

  angleActivityLevel = normalizedDistance;

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

  initLeds();
  initChainDevices();
  initChainLeds();

  // USB-Cを背面とした正面から
  // DualKey左 -> DualKey右 -> Encoder -> Angle の順で起動演出
  playBootLedAnimation();

  // 起動時はAngleを物理的な中央位置に置いておく
  calibrateAngleCenter();

  // 起動後の通常LED状態を反映
  updateLeds();
}


// ============================================================
// DualKey processing
// ============================================================

void updateDualKey() {
  const uint32_t now = millis();

  Key1.setRawState(now, !digitalRead(PIN_KEY_LEFT));
  Key2.setRawState(now, !digitalRead(PIN_KEY_RIGHT));

  const bool leftPressed = Key1.isPressed();
  const bool rightPressed = Key2.isPressed();

  // 両押し最優先
  if (leftPressed && rightPressed) {
    if (!chordConsumed) {
      pending = PendingKey::NONE;

      sendAudioToggle();
      toggleAudioOutputState();

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
      sendOra4();
      setAudioOutputState(AudioOutput::ORA4);
      singleConsumed = true;
    } else if (pending == PendingKey::RIGHT && rightPressed) {
      sendStudioDisplay();
      setAudioOutputState(AudioOutput::STUDIO_DISPLAY);
      singleConsumed = true;
    }

    pending = PendingKey::NONE;
  }

  if (pending == PendingKey::LEFT && Key1.wasReleased()) {

    sendOra4();
    setAudioOutputState(AudioOutput::ORA4);
    pending = PendingKey::NONE;
    singleConsumed = true;
  }

  if (pending == PendingKey::RIGHT && Key2.wasReleased()) {

    sendStudioDisplay();
    setAudioOutputState(AudioOutput::STUDIO_DISPLAY);
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
      toggleMuteState();
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
  updateLeds();

  delay(2);
}

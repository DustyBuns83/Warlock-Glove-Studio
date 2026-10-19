#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <SparkFunLSM6DS3.h>
#include <math.h>

// ======================================
// WARLOCK GLOVE V4
// Three modes - No buttons
// XIAO ESP32-C3 + LSM6DS3
// ======================================

// CONFIRMED WORKING GPIO PINS
#define THUMB_PIN    3
#define POINTER_PIN  4
#define MIDDLE_PIN   5
#define RING_PIN     6
#define PINKY_PIN    21

#define SDA_PIN 9
#define SCL_PIN 10

#define NUM_FINGERS 5
#define NUM_LEDS 6

// ======================================
// BRIGHTNESS / POWER
// ======================================

#define FIRE_BRIGHTNESS 85
#define MAGIC_BRIGHTNESS 85
#define LIGHTNING_BRIGHTNESS 100

#define FRAME_TIME 40

// ======================================
// GESTURE CONFIGURATION
// ======================================

// Our previously measured flick:
// 2.5-3g and 800-1000 deg/sec

#define ACCEL_THRESHOLD 2.0
#define GYRO_THRESHOLD 600.0

// IMPORTANT:
// Select which signed gyro axis corresponds
// to the outward flick.
//
// 0 = X
// 1 = Y
// 2 = Z
//
// First test with the Serial Monitor.
// Change these if necessary.

#define FLICK_AXIS 0
#define FLICK_DIRECTION 1

// Direction is +1 or -1.

// Gesture timing
#define FLICK_RELEASE_MS 180
#define FLICK_GROUP_GAP_MS 650
#define GESTURE_COOLDOWN_MS 1200

// ======================================
// FIRE CONFIGURATION
// ======================================

#define IGNITION_CHANCE 12

// ======================================
// LED OBJECTS
// ======================================

Adafruit_NeoPixel thumb(
  NUM_LEDS, THUMB_PIN, NEO_GRB + NEO_KHZ800
);

Adafruit_NeoPixel pointer(
  NUM_LEDS, POINTER_PIN, NEO_GRB + NEO_KHZ800
);

Adafruit_NeoPixel middle(
  NUM_LEDS, MIDDLE_PIN, NEO_GRB + NEO_KHZ800
);

Adafruit_NeoPixel ringFinger(
  NUM_LEDS, RING_PIN, NEO_GRB + NEO_KHZ800
);

Adafruit_NeoPixel pinky(
  NUM_LEDS, PINKY_PIN, NEO_GRB + NEO_KHZ800
);

Adafruit_NeoPixel* fingers[NUM_FINGERS] = {
  &thumb,
  &pointer,
  &middle,
  &ringFinger,
  &pinky
};

LSM6DS3 imu(I2C_MODE, 0x6B);

// ======================================
// MODES
// ======================================

enum GloveMode {
  FIRE_MODE,
  LIGHTNING_MODE,
  MAGIC_MODE
};

GloveMode currentMode = FIRE_MODE;

enum GloveState {
  STANDBY,
  CASTING,
  TRANSITIONING
};

GloveState gloveState = STANDBY;

// ======================================
// FIRE DATA - PRESERVED FROM V2
// ======================================

struct Flame {
  bool active;
  float position;
  float speed;
  float intensity;
  float decay;
  int maxHeight;
};

Flame flames[NUM_FINGERS];

// ======================================
// GESTURE DATA
// ======================================

int flickCount = 0;

bool flickLatched = false;

unsigned long lastFlickTime = 0;
unsigned long releaseStart = 0;
unsigned long gestureCooldownUntil = 0;

float gyroValues[3] = {0, 0, 0};

// ======================================
// TIMING
// ======================================

unsigned long lastFrame = 0;

// ======================================
// HELPERS
// ======================================

void showAll() {
  for (int f = 0; f < NUM_FINGERS; f++) {
    fingers[f]->show();
  }
}

void clearAll() {
  for (int f = 0; f < NUM_FINGERS; f++) {
    fingers[f]->clear();
  }
  showAll();
}

void setAll(uint8_t r, uint8_t g, uint8_t b) {
  for (int f = 0; f < NUM_FINGERS; f++) {
    fingers[f]->fill(
      fingers[f]->Color(r, g, b)
    );
  }
  showAll();
}

void setBrightnessAll(uint8_t brightness) {
  for (int f = 0; f < NUM_FINGERS; f++) {
    fingers[f]->setBrightness(brightness);
  }
}

void setPixelAll(
  int led,
  uint8_t r,
  uint8_t g,
  uint8_t b
) {
  for (int f = 0; f < NUM_FINGERS; f++) {
    fingers[f]->setPixelColor(
      led,
      fingers[f]->Color(r, g, b)
    );
  }
}

// ======================================
// FIRE COLOR - V2
// ======================================

uint32_t fireColor(
  Adafruit_NeoPixel* strip,
  float intensity
) {

  intensity = constrain(intensity, 0, 255);

  uint8_t red;
  uint8_t green;
  uint8_t blue = 0;

  if (intensity < 80) {
    red = intensity * 1.5;
    green = intensity * 0.12;
  }
  else if (intensity < 170) {
    red = intensity;
    green = (intensity - 50) * 0.35;
  }
  else {
    red = 255;
    green = (intensity - 100) * 0.65;
  }

  return strip->Color(red, green, blue);
}

// ======================================
// FIRE IGNITION - V2
// ======================================

void igniteFlame(int f) {

  flames[f].active = true;
  flames[f].position = 1.5;

  flames[f].speed = random(5, 13) / 100.0;
  flames[f].intensity = random(100, 190);
  flames[f].decay = random(2, 6) / 10.0;

  int chance = random(100);

  if (chance < 35) {
    flames[f].maxHeight = 3;
  }
  else if (chance < 75) {
    flames[f].maxHeight = 4;
  }
  else {
    flames[f].maxHeight = 5;
  }
}

// ======================================
// BASE FIRE - V2
// ======================================

void drawBaseFire(Adafruit_NeoPixel* strip) {

  float base1 = random(45, 90);
  float base2 = random(75, 140);
  float base3 = random(15, 95);

  strip->setPixelColor(
    0, fireColor(strip, base1)
  );

  strip->setPixelColor(
    1, fireColor(strip, base2)
  );

  strip->setPixelColor(
    2, fireColor(strip, base3)
  );
}

// ======================================
// FIRE STANDBY - V2
// ======================================

void updateFire() {

  for (int f = 0; f < NUM_FINGERS; f++) {

    Adafruit_NeoPixel* strip = fingers[f];

    strip->clear();
    drawBaseFire(strip);

    Flame &flame = flames[f];

    if (!flame.active) {
      if (random(1000) < IGNITION_CHANCE) {
        igniteFlame(f);
      }
    }

    if (flame.active) {

      flame.position += flame.speed;

      if (flame.position < 3.5) {
        flame.intensity += random(1, 5);
      }
      else {
        flame.intensity -= flame.decay * 3;
      }

      flame.intensity = constrain(
        flame.intensity, 0, 230
      );

      for (int i = 2; i < NUM_LEDS; i++) {

        float distance = flame.position - i;
        float brightness = 0;

        if (
          distance >= -0.5 &&
          distance <= 0.5
        ) {
          brightness = flame.intensity;
        }
        else if (
          distance > 0.5 &&
          distance < 2.5
        ) {
          brightness =
            flame.intensity *
            (1.0 - distance / 2.5);
        }

        if (brightness > 0) {
          brightness *=
            random(75, 111) / 100.0;
        }

        brightness = constrain(
          brightness, 0, 255
        );

        if (brightness > 0) {
          strip->setPixelColor(
            i,
            fireColor(strip, brightness)
          );
        }
      }

      if (
        flame.position > flame.maxHeight ||
        flame.intensity < 15
      ) {
        flame.active = false;
      }
    }

    strip->show();
  }
}

// ======================================
// LIGHTNING STANDBY
// Slow crystal-blue breathing
// ======================================

void updateLightningStandby() {

  float t = millis() / 1000.0;

  float pulse =
    (sin(t * 2.3) + 1.0) * 0.5;

  int base = 12 + pulse * 50;

  for (int f = 0; f < NUM_FINGERS; f++) {

    Adafruit_NeoPixel* strip = fingers[f];

    for (int i = 0; i < NUM_LEDS; i++) {

      int flicker = random(-8, 9);

      int brightness = constrain(
        base + flicker,
        2,
        90
      );

      // Small, occasional electrical sparks
      if (random(1000) < 12) {
        brightness = random(100, 170);
      }

      strip->setPixelColor(
        i,
        strip->Color(
          brightness / 10,
          brightness / 2,
          brightness
        )
      );
    }

    strip->show();
  }
}

// ======================================
// MAGIC STANDBY
// Independent rainbow color breathing
// ======================================

void updateMagicStandby() {

  float t = millis() / 1000.0;

  for (int f = 0; f < NUM_FINGERS; f++) {

    Adafruit_NeoPixel* strip = fingers[f];

    for (int i = 0; i < NUM_LEDS; i++) {

      float wave =
        (sin(t * 1.5 + f * 1.4 + i * 0.9)
        + 1.0) * 0.5;

      uint16_t hue =
        (uint16_t)(
          millis() * 8 +
          f * 11000 +
          i * 5500
        );

      uint8_t value =
        35 + wave * 145;

      uint32_t color =
        strip->ColorHSV(
          hue,
          255,
          value
        );

      strip->setPixelColor(
        i,
        strip->gamma32(color)
      );
    }

    strip->show();
  }
}

// ======================================
// FIREBALL SPELL
// ======================================

void castFireball() {

  gloveState = CASTING;

  setBrightnessAll(FIRE_BRIGHTNESS);

  // Gather fire energy
  for (int stage = 0; stage < 15; stage++) {

    int intensity = 35 + stage * 12;

    clearAll();

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      strip->setPixelColor(
        0,
        strip->Color(
          intensity,
          intensity / 5,
          0
        )
      );

      strip->setPixelColor(
        1,
        strip->Color(
          intensity,
          intensity / 3,
          0
        )
      );

      strip->setPixelColor(
        2,
        strip->Color(
          intensity,
          intensity / 4,
          0
        )
      );
    }

    showAll();
    delay(35);
  }

  // Fireball shoots toward fingertips
  for (int led = 0; led < NUM_LEDS; led++) {

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      strip->clear();

      strip->setPixelColor(
        led,
        strip->Color(255, 170, 12)
      );

      if (led > 0) {
        strip->setPixelColor(
          led - 1,
          strip->Color(220, 50, 0)
        );
      }

      if (led > 1) {
        strip->setPixelColor(
          led - 2,
          strip->Color(90, 5, 0)
        );
      }
    }

    showAll();
    delay(45);
  }

  // Ember fade
  for (int b = 100; b >= 0; b -= 5) {

    setAll(b * 2, b / 3, 0);
    delay(20);
  }

  clearAll();

  gloveState = STANDBY;
}

// ======================================
// LIGHTNING CHARGE - V3 STYLE
// ======================================

void lightningCharge() {

  unsigned long start = millis();

  while (millis() - start < 1500) {

    float progress =
      (millis() - start) / 1500.0;

    int brightness =
      20 + progress * 110;

    brightness += random(-20, 25);

    brightness = constrain(
      brightness, 5, 150
    );

    setAll(
      0,
      brightness * 0.65,
      brightness
    );

    delay(40 - progress * 25);
  }
}

// ======================================
// LIGHTNING STRIKE - V3 STYLE
// ======================================

void lightningStrike() {

  for (int led = 0; led < NUM_LEDS; led++) {

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      strip->clear();

      strip->setPixelColor(
        led,
        strip->Color(255, 255, 255)
      );

      if (led > 0) {
        strip->setPixelColor(
          led - 1,
          strip->Color(20, 90, 255)
        );
      }

      if (led > 1) {
        strip->setPixelColor(
          led - 2,
          strip->Color(0, 20, 90)
        );
      }

      strip->show();
    }

    delay(20);
  }

  clearAll();
}

// ======================================
// COMPLETE LIGHTNING SPELL
// ======================================

void castLightning() {

  gloveState = CASTING;

  clearAll();

  setBrightnessAll(LIGHTNING_BRIGHTNESS);

  lightningCharge();

  for (int strike = 0; strike < 3; strike++) {

    lightningStrike();

    delay(100);
  }

  for (int b = 80; b >= 0; b -= 4) {
    setAll(0, b / 2, b);
    delay(20);
  }

  clearAll();

  gloveState = STANDBY;
}

// ======================================
// MAGIC BLAST
// ======================================

void castMagicBlast() {

  gloveState = CASTING;

  setBrightnessAll(MAGIC_BRIGHTNESS);

  // Energy gathering
  for (int stage = 0; stage < 18; stage++) {

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      for (int i = 0; i < NUM_LEDS; i++) {

        uint16_t hue =
          (uint16_t)(
            millis() * 20 +
            f * 9000 +
            i * 6000
          );

        int value = 40 + stage * 11;

        strip->setPixelColor(
          i,
          strip->ColorHSV(
            hue,
            255,
            constrain(value, 0, 255)
          )
        );
      }
    }

    showAll();
    delay(35);
  }

  // Rainbow energy wave
  for (int led = 0; led < NUM_LEDS; led++) {

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      strip->clear();

      uint16_t hue =
        (uint16_t)(
          f * 12000 +
          led * 7500
        );

      strip->setPixelColor(
        led,
        strip->ColorHSV(
          hue,
          180,
          255
        )
      );

      if (led > 0) {
        strip->setPixelColor(
          led - 1,
          strip->ColorHSV(
            hue + 5000,
            255,
            130
          )
        );
      }
    }

    showAll();
    delay(55);
  }

  // Shimmering fingertips
  for (int frame = 0; frame < 12; frame++) {

    clearAll();

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      strip->setPixelColor(
        NUM_LEDS - 1,
        strip->ColorHSV(
          random(65536),
          150,
          random(100, 255)
        )
      );
    }

    showAll();
    delay(45);
  }

  clearAll();

  gloveState = STANDBY;
}

// ======================================
// MODE TRANSITION
// ======================================

void transitionMode() {

  gloveState = TRANSITIONING;

  clearAll();

  currentMode =
    (GloveMode)((currentMode + 1) % 3);

  if (currentMode == FIRE_MODE) {

    setBrightnessAll(FIRE_BRIGHTNESS);

    for (int i = 0; i < NUM_LEDS; i++) {

      clearAll();

      for (int j = 0; j <= i; j++) {
        setPixelAll(j, 180, 30, 0);
      }

      showAll();
      delay(75);
    }

    Serial.println("MODE: FIRE");
  }

  else if (currentMode == LIGHTNING_MODE) {

    setBrightnessAll(LIGHTNING_BRIGHTNESS);

    for (int i = 0; i < NUM_LEDS; i++) {

      clearAll();

      setPixelAll(i, 0, 130, 255);

      showAll();
      delay(65);
    }

    Serial.println("MODE: LIGHTNING");
  }

  else {

    setBrightnessAll(MAGIC_BRIGHTNESS);

    for (int i = 0; i < NUM_LEDS; i++) {

      clearAll();

      for (int f = 0; f < NUM_FINGERS; f++) {

        fingers[f]->setPixelColor(
          i,
          fingers[f]->ColorHSV(
            f * 12000 + i * 6000,
            255,
            255
          )
        );
      }

      showAll();
      delay(75);
    }

    Serial.println("MODE: MAGIC HANDS");
  }

  clearAll();

  gloveState = STANDBY;
}

// ======================================
// CAST CURRENT SPELL
// ======================================

void castCurrentSpell() {

  if (currentMode == FIRE_MODE) {
    Serial.println("CAST: FIREBALL");
    castFireball();
  }

  else if (currentMode == LIGHTNING_MODE) {
    Serial.println("CAST: LIGHTNING");
    castLightning();
  }

  else {
    Serial.println("CAST: MAGIC BLAST");
    castMagicBlast();
  }
}

// ======================================
// READ GESTURE
// ======================================

void updateGesture() {

  if (gloveState != STANDBY) return;

  unsigned long now = millis();

  if (now < gestureCooldownUntil) return;

  float ax = imu.readFloatAccelX();
  float ay = imu.readFloatAccelY();
  float az = imu.readFloatAccelZ();

  float gx = imu.readFloatGyroX();
  float gy = imu.readFloatGyroY();
  float gz = imu.readFloatGyroZ();

  gyroValues[0] = gx;
  gyroValues[1] = gy;
  gyroValues[2] = gz;

  float acceleration =
    sqrt(ax * ax + ay * ay + az * az);

  float signedRotation =
    gyroValues[FLICK_AXIS] *
    FLICK_DIRECTION;

  float rotationMagnitude =
    sqrt(gx * gx + gy * gy + gz * gz);

  // A qualifying outward flick
  bool flickDetected =
    acceleration > ACCEL_THRESHOLD &&
    rotationMagnitude > GYRO_THRESHOLD &&
    signedRotation > GYRO_THRESHOLD;

  // Re-arm only after the selected axis
  // has settled for a short interval.
  bool settled =
    fabs(gyroValues[FLICK_AXIS]) < 100.0;

  if (!flickLatched && flickDetected) {

    flickLatched = true;

    flickCount++;
    lastFlickTime = now;
    releaseStart = 0;

    Serial.print("FLICK COUNT: ");
    Serial.println(flickCount);

    if (flickCount >= 3) {

      flickCount = 0;

      transitionMode();

      gestureCooldownUntil =
        millis() + GESTURE_COOLDOWN_MS;

      flickLatched = true;
      releaseStart = 0;

      return;
    }
  }

  if (flickLatched) {

    if (settled) {

      if (releaseStart == 0) {
        releaseStart = now;
      }

      if (
        now - releaseStart >=
        FLICK_RELEASE_MS
      ) {
        flickLatched = false;
        releaseStart = 0;
      }
    }
    else {
      releaseStart = 0;
    }
  }

  // Interpret the completed sequence
  if (
    flickCount > 0 &&
    now - lastFlickTime >
    FLICK_GROUP_GAP_MS
  ) {

    int completedFlicks = flickCount;

    flickCount = 0;

    if (completedFlicks == 1) {

      castCurrentSpell();

      gestureCooldownUntil =
        millis() + GESTURE_COOLDOWN_MS;

      flickLatched = true;
      releaseStart = 0;
    }
    else {
      Serial.println(
        "Two flicks: sequence canceled"
      );
    }
  }
}

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (imu.begin() != 0) {
    Serial.println(
      "LSM6DS3 initialization failed!"
    );
    while (1) {
      delay(100);
    }
  }

  randomSeed(micros());

  for (int f = 0; f < NUM_FINGERS; f++) {

    fingers[f]->begin();
    fingers[f]->setBrightness(
      FIRE_BRIGHTNESS
    );

    fingers[f]->clear();
    fingers[f]->show();

    flames[f].active = false;
    flames[f].position = 0;
    flames[f].intensity = 0;
  }

  Serial.println();
  Serial.println("======================");
  Serial.println("WARLOCK GLOVE V4 READY");
  Serial.println("======================");

  Serial.println("Starting mode: FIRE");
  Serial.println("One flick: Cast spell");
  Serial.println("Three flicks: Next mode");
}

// ======================================
// MAIN LOOP
// ======================================

void loop() {

  updateGesture();

  unsigned long now = millis();

  if (
    gloveState == STANDBY &&
    now - lastFrame >= FRAME_TIME
  ) {

    lastFrame = now;

    if (currentMode == FIRE_MODE) {
      updateFire();
    }

    else if (currentMode == LIGHTNING_MODE) {
      updateLightningStandby();
    }

    else {
      updateMagicStandby();
    }
  }

  delay(5);
}
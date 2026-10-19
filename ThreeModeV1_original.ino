#include <Wire.h>

#include <Adafruit_NeoPixel.h>

#include <SparkFunLSM6DS3.h>

#include <math.h>

// ============================================

// WARLOCK GLOVE - FIRE & LIGHTNING V3

// XIAO ESP32-C3

// ============================================

// GPIO numbers

#define THUMB_PIN    3

#define POINTER_PIN  4

#define MIDDLE_PIN   5

#define RING_PIN     6

#define PINKY_PIN    21

#define SDA_PIN 9

#define SCL_PIN 10

#define NUM_LEDS 6

#define NUM_FINGERS 5

// ============================================

// FIRE SETTINGS

// ============================================

#define MAX_BRIGHTNESS 85

#define FRAME_TIME 40

#define IGNITION_CHANCE 12

// ============================================

// GESTURE SETTINGS

// ============================================

#define ACCEL_THRESHOLD 2.0

#define GYRO_THRESHOLD 600.0

#define GESTURE_COOLDOWN 2000

// ============================================

// LIGHTNING SETTINGS

// ============================================

#define CHARGE_DURATION 1500

#define STRIKE_STEP 20

#define STRIKE_PAUSE 100

#define STRIKE_COUNT 3

// Keep white flashes limited to avoid

// excessive current draw from 30 LEDs.

#define LIGHTNING_BRIGHTNESS 100

// ============================================

// LED STRIPS

// ============================================

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

// ============================================

// ACCELEROMETER

// ============================================

LSM6DS3 imu(I2C_MODE, 0x6B);

// ============================================

// FIRE DATA

// ============================================

struct Flame {

  bool active;

  float position;

  float speed;

  float intensity;

  float decay;

  int maxHeight;

};

Flame flames[NUM_FINGERS];

unsigned long lastGesture = 0;

// ============================================

// FIRE COLOR

// ============================================

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

// ============================================

// START FIRE FLAME

// ============================================

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

// ============================================

// CONSTANT BASE FIRE

// ============================================

void drawBaseFire(

  Adafruit_NeoPixel* strip

) {

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

// ============================================

// UPDATE FIRE

// ============================================

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

// ============================================

// SET ALL FINGERS TO ONE COLOR

// ============================================

void setAll(

  uint8_t r,

  uint8_t g,

  uint8_t b

) {

  for (int f = 0; f < NUM_FINGERS; f++) {

    fingers[f]->fill(

      fingers[f]->Color(r, g, b)

    );

    fingers[f]->show();

  }

}

// ============================================

// CRYSTAL BLUE CHARGING

// ============================================

void lightningCharge() {

  unsigned long start = millis();

  while (millis() - start < CHARGE_DURATION) {

    float progress =

      (millis() - start) /

      (float)CHARGE_DURATION;

    // Increasing charge intensity

    int brightness =

      20 + progress * 110;

    // Rapid electrical fluctuations

    brightness += random(-20, 25);

    brightness = constrain(

      brightness, 5, 150

    );

    // Crystal blue

    uint8_t r = 0;

    uint8_t g = brightness * 0.65;

    uint8_t b = brightness;

    setAll(r, g, b);

    delay(40 - progress * 25);

  }

}

// ============================================

// SINGLE LIGHTNING STRIKE

// ============================================

void lightningStrike() {

  for (int led = 0; led < NUM_LEDS; led++) {

    for (int f = 0; f < NUM_FINGERS; f++) {

      Adafruit_NeoPixel* strip = fingers[f];

      strip->clear();

      // Bright white leading edge

      strip->setPixelColor(

        led,

        strip->Color(255, 255, 255)

      );

      // Short blue trail

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

    delay(STRIKE_STEP);

  }

  setAll(0, 0, 0);

}

// ============================================

// COMPLETE LIGHTNING SPELL

// ============================================

void castLightning() {

  // Stop fire

  setAll(0, 0, 0);

  // Increase brightness for lightning

  for (int f = 0; f < NUM_FINGERS; f++) {

    fingers[f]->setBrightness(

      LIGHTNING_BRIGHTNESS

    );

  }

  lightningCharge();

  // Triple lightning discharge

  for (int strike = 0;

       strike < STRIKE_COUNT;

       strike++) {

    lightningStrike();

    delay(STRIKE_PAUSE);

  }

  // Fade remaining electrical energy

  for (int b = 80; b >= 0; b -= 4) {

    setAll(0, b / 2, b);

    delay(20);

  }

  setAll(0, 0, 0);

  // Restore fire brightness

  for (int f = 0; f < NUM_FINGERS; f++) {

    fingers[f]->setBrightness(

      MAX_BRIGHTNESS

    );

  }

}

// ============================================

// DETECT WRIST FLICK

// ============================================

bool detectGesture() {

  float ax = imu.readFloatAccelX();

  float ay = imu.readFloatAccelY();

  float az = imu.readFloatAccelZ();

  float gx = imu.readFloatGyroX();

  float gy = imu.readFloatGyroY();

  float gz = imu.readFloatGyroZ();

  float acceleration =

    sqrt(ax * ax + ay * ay + az * az);

  float rotation =

    sqrt(gx * gx + gy * gy + gz * gz);

  if (

    acceleration > ACCEL_THRESHOLD &&

    rotation > GYRO_THRESHOLD

  ) {

    return true;

  }

  return false;

}

// ============================================

// SETUP

// ============================================

void setup() {

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (imu.begin() != 0) {

    Serial.println(

      "Accelerometer initialization failed!"

    );

    while (1);

  }

  randomSeed(micros());

  for (int f = 0; f < NUM_FINGERS; f++) {

    fingers[f]->begin();

    fingers[f]->setBrightness(

      MAX_BRIGHTNESS

    );

    fingers[f]->clear();

    fingers[f]->show();

    flames[f].active = false;

    flames[f].position = 0;

    flames[f].intensity = 0;

  }

  Serial.println("Warlock Glove Ready!");

}

// ============================================

// MAIN LOOP

// ============================================

void loop() {

  updateFire();

  // Prevent repeated triggers

  if (

    millis() - lastGesture >

    GESTURE_COOLDOWN

  ) {

    if (detectGesture()) {

      Serial.println("LIGHTNING!");

      castLightning();

      lastGesture = millis();

    }

  }

  delay(FRAME_TIME);

}
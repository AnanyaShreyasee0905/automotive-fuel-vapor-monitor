// Fuel-vapor monitor - Phase 1b: speed-indexed baseline calibration
// Calibrate a gas baseline at several simulated speeds (via pot), store
// them in flash, and interpolate a dynamic baseline live based on current
// speed. Hysteresis (separate enter/clear margins) makes the alert state
// clear faster than waiting for full sensor recovery.
//
// CONTROLS:
//   Short press mute button  -> mute/unmute buzzer
//   Long press (>2s)         -> capture a new calibration point at current
//                                pot position (keep sensor in clean air!)
//
// Calibration points are saved to flash (Preferences) so they survive
// power cycles. Do a few points across the pot's range before relying
// on the interpolated baseline.

#include <Preferences.h>

const int MQ2_PIN = 34;
const int POT_PIN = 35;

const int LED_R = 25;
const int LED_G = 26;
const int LED_B = 27;
const int BUZZER_PIN = 14;
const int MUTE_BUTTON_PIN = 13;

const int COLOR_ON = HIGH;   // common cathode
const int COLOR_OFF = LOW;

const unsigned long WARMUP_MS = 30000;
const unsigned long CAL_SAMPLE_MS = 3000;   // how long to average during calibration
const unsigned long LONG_PRESS_MS = 2000;   // hold time to trigger calibration

const int ALERT_MARGIN = 400;   // enter alert this far above interpolated baseline
const int CLEAR_MARGIN = 150;   // drop out of alert once back under this (hysteresis)
const int WARN_MARGIN = 200;

const int MAX_POINTS = 10;
struct CalPoint {
  float speed;
  int baseline;
};
CalPoint calPoints[MAX_POINTS];
int numCalPoints = 0;

Preferences prefs;

unsigned long startTime;
bool warmedUp = false;

bool lastButtonState = HIGH;
unsigned long buttonDownTime = 0;
bool longPressFired = false;
bool muted = false;

bool inAlert = false;

const int SMOOTH_N = 5;
int samples[SMOOTH_N];
int sampleIndex = 0;
bool samplesFilled = false;

int smoothedGasRead() {
  samples[sampleIndex] = analogRead(MQ2_PIN);
  sampleIndex = (sampleIndex + 1) % SMOOTH_N;
  if (sampleIndex == 0) samplesFilled = true;
  int count = samplesFilled ? SMOOTH_N : sampleIndex;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += samples[i];
  return sum / count;
}

float readSpeed() {
  int potRaw = analogRead(POT_PIN);
  return map(potRaw, 0, 4095, 0, 120);  // 0-120 km/h simulated
}

void setColor(bool r, bool g, bool b) {
  digitalWrite(LED_R, r ? COLOR_ON : COLOR_OFF);
  digitalWrite(LED_G, g ? COLOR_ON : COLOR_OFF);
  digitalWrite(LED_B, b ? COLOR_ON : COLOR_OFF);
}

void loadCalibration() {
  prefs.begin("calib", true);
  numCalPoints = prefs.getInt("count", 0);
  if (numCalPoints > MAX_POINTS) numCalPoints = MAX_POINTS;
  for (int i = 0; i < numCalPoints; i++) {
    String sKey = "s" + String(i);
    String bKey = "b" + String(i);
    calPoints[i].speed = prefs.getFloat(sKey.c_str(), 0);
    calPoints[i].baseline = prefs.getInt(bKey.c_str(), 0);
  }
  prefs.end();
  Serial.print("Loaded ");
  Serial.print(numCalPoints);
  Serial.println(" calibration point(s) from flash.");
}

void saveCalibration() {
  prefs.begin("calib", false);
  prefs.putInt("count", numCalPoints);
  for (int i = 0; i < numCalPoints; i++) {
    String sKey = "s" + String(i);
    String bKey = "b" + String(i);
    prefs.putFloat(sKey.c_str(), calPoints[i].speed);
    prefs.putInt(bKey.c_str(), calPoints[i].baseline);
  }
  prefs.end();
}

void addCalPoint(float speed, int baseline) {
  for (int i = 0; i < numCalPoints; i++) {
    if (abs(calPoints[i].speed - speed) < 5) {
      calPoints[i].baseline = baseline;
      saveCalibration();
      Serial.println("Updated existing calibration point.");
      return;
    }
  }
  if (numCalPoints >= MAX_POINTS) {
    Serial.println("Calibration table full, cannot add more points.");
    return;
  }
  calPoints[numCalPoints].speed = speed;
  calPoints[numCalPoints].baseline = baseline;
  numCalPoints++;

  for (int i = numCalPoints - 1; i > 0; i--) {
    if (calPoints[i].speed < calPoints[i - 1].speed) {
      CalPoint tmp = calPoints[i];
      calPoints[i] = calPoints[i - 1];
      calPoints[i - 1] = tmp;
    } else break;
  }
  saveCalibration();
  Serial.println("New calibration point added and saved.");
}

int interpolateBaseline(float speed) {
  if (numCalPoints == 0) return 0;
  if (numCalPoints == 1) return calPoints[0].baseline;

  if (speed <= calPoints[0].speed) return calPoints[0].baseline;
  if (speed >= calPoints[numCalPoints - 1].speed) return calPoints[numCalPoints - 1].baseline;

  for (int i = 0; i < numCalPoints - 1; i++) {
    float s0 = calPoints[i].speed;
    float s1 = calPoints[i + 1].speed;
    if (speed >= s0 && speed <= s1) {
      float t = (s1 == s0) ? 0 : (speed - s0) / (s1 - s0);
      return calPoints[i].baseline + t * (calPoints[i + 1].baseline - calPoints[i].baseline);
    }
  }
  return calPoints[numCalPoints - 1].baseline;
}

void runCalibrationCapture() {
  float speedNow = readSpeed();
  Serial.print("Calibrating at speed ");
  Serial.print(speedNow);
  Serial.println(" km/h - keep sensor in clean air...");

  setColor(false, false, true);
  long sum = 0;
  int count = 0;
  unsigned long start = millis();
  while (millis() - start < CAL_SAMPLE_MS) {
    sum += analogRead(MQ2_PIN);
    count++;
    delay(50);
  }
  int newBaseline = sum / count;
  addCalPoint(speedNow, newBaseline);
  Serial.print("Captured baseline ");
  Serial.print(newBaseline);
  Serial.print(" at ");
  Serial.print(speedNow);
  Serial.println(" km/h.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MUTE_BUTTON_PIN, INPUT_PULLUP);

  setColor(false, false, false);
  digitalWrite(BUZZER_PIN, LOW);

  loadCalibration();

  Serial.println("MQ-2 warming up...");
  startTime = millis();
}

void loop() {
  unsigned long elapsed = millis() - startTime;
  int gasRaw = smoothedGasRead();
  float speedNow = readSpeed();

  if (!warmedUp) {
    if (elapsed < WARMUP_MS) {
      setColor(false, false, true);
      Serial.print("Warming up, ");
      Serial.print((WARMUP_MS - elapsed) / 1000);
      Serial.println("s left");
      delay(500);
      return;
    }
    warmedUp = true;
    Serial.println("Warm-up complete.");
  }

  bool buttonReading = digitalRead(MUTE_BUTTON_PIN);
  if (buttonReading == LOW && lastButtonState == HIGH) {
    buttonDownTime = millis();
    longPressFired = false;
  }
  if (buttonReading == LOW && !longPressFired && (millis() - buttonDownTime > LONG_PRESS_MS)) {
    longPressFired = true;
    runCalibrationCapture();
  }
  if (buttonReading == HIGH && lastButtonState == LOW) {
    if (!longPressFired) {
      muted = !muted;
      Serial.println(muted ? "Buzzer MUTED" : "Buzzer UNMUTED");
    }
  }
  lastButtonState = buttonReading;

  if (numCalPoints == 0) {
    setColor(false, true, false);
    Serial.print("No calibration yet. Raw: ");
    Serial.print(gasRaw);
    Serial.print("  Speed: ");
    Serial.print(speedNow);
    Serial.println(" km/h. Hold mute button 2s at this speed to calibrate.");
    delay(300);
    return;
  }

  int dynBaseline = interpolateBaseline(speedNow);
  int delta = gasRaw - dynBaseline;

  if (!inAlert && delta > ALERT_MARGIN) {
    inAlert = true;
  } else if (inAlert && delta < CLEAR_MARGIN) {
    inAlert = false;
  }

  if (inAlert) {
    setColor(true, false, false);
    digitalWrite(BUZZER_PIN, muted ? LOW : HIGH);
  } else if (delta > WARN_MARGIN) {
    setColor(true, true, false);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    setColor(false, true, false);
    digitalWrite(BUZZER_PIN, LOW);
  }

  Serial.print("Gas: ");
  Serial.print(gasRaw);
  Serial.print("  Speed: ");
  Serial.print(speedNow);
  Serial.print("  DynBaseline: ");
  Serial.print(dynBaseline);
  Serial.print("  Delta: ");
  Serial.print(delta);
  Serial.print("  State: ");
  Serial.print(inAlert ? "ALERT" : (delta > WARN_MARGIN ? "WARN" : "clean"));
  Serial.println(muted ? "  [MUTED]" : "");

  delay(300);
}

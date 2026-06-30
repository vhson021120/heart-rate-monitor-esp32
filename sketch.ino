/*
  heart_monitor_pulldown.ino
  Sử dụng INPUT_PULLDOWN: nút nối tới 3V3 (VCC)
  Khi bấm nút: GPIO sẽ thay đổi từ LOW -> HIGH
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_system.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pins
const int PIN_POWER   = 26;
const int PIN_MEASURE = 27;
const int PIN_RESET   = 25;
const int PIN_LED     = 18;
const int PIN_BUZZER  = 19;

// simple debounce
const unsigned long DEBOUNCE_MS = 50;
struct SimpleBtn {
  int pin;
  bool lastStable;
  unsigned long lastChangeMillis;
  bool lastEventReported;
};
SimpleBtn Bpower  = {PIN_POWER, LOW, 0, false};
SimpleBtn Bmeasure= {PIN_MEASURE, LOW, 0, false};
SimpleBtn Breset  = {PIN_RESET, LOW, 0, false};

enum State {OFF, WAIT, MEASURING, SHOW_RESULT};
State state = OFF;

unsigned long measureStart = 0;
const unsigned long MEASURE_DURATION = 3000;
const unsigned long UPDATE_INTERVAL = 250;
unsigned long lastUpdate = 0;
int currentBpm = 60;
int finalBpm = 0;

const int START_MIN = 55;
const int START_MAX = 79;
const int BPM_MIN = 30;
const int BPM_MAX = 200;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // INPUT_PULLDOWN: khi không bấm = LOW, khi bấm = HIGH (nối tới 3V3)
  pinMode(PIN_POWER, INPUT_PULLDOWN);
  pinMode(PIN_MEASURE, INPUT_PULLDOWN);
  pinMode(PIN_RESET, INPUT_PULLDOWN);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Initialize button state from actual pin readings
  Bpower.lastStable = digitalRead(Bpower.pin);
  Bpower.lastChangeMillis = millis();
  Bpower.lastEventReported = (Bpower.lastStable == HIGH);

  Bmeasure.lastStable = digitalRead(Bmeasure.pin);
  Bmeasure.lastChangeMillis = millis();
  Bmeasure.lastEventReported = (Bmeasure.lastStable == HIGH);

  Breset.lastStable = digitalRead(Breset.pin);
  Breset.lastChangeMillis = millis();
  Breset.lastEventReported = (Breset.lastStable == HIGH);

  Serial.println("Ready (INPUT_PULLDOWN mode):");
  Serial.printf("POWER initial=%d  MEASURE initial=%d  RESET initial=%d\n",
                Bpower.lastStable, Bmeasure.lastStable, Breset.lastStable);

  randomSeed((unsigned long)esp_random());

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed");
  }
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
}

// returns true once when a press edge (HIGH) is detected
bool readButtonPress(SimpleBtn &b) {
  bool raw = digitalRead(b.pin); // HIGH = pressed (3V3), LOW = not pressed
  unsigned long now = millis();
  if (raw != b.lastStable) {
    if (now - b.lastChangeMillis > DEBOUNCE_MS) {
      b.lastStable = raw;
      b.lastChangeMillis = now;
      if (b.lastStable == HIGH) { // pressed
        if (!b.lastEventReported) {
          b.lastEventReported = true;
          return true;
        } else {
          return false;
        }
      } else { // released
        b.lastEventReported = false;
        return false;
      }
    }
  } else {
    b.lastChangeMillis = now;
  }
  return false;
}

void loop() {
  if (readButtonPress(Bpower)) {
    Serial.println("POWER pressed");
    if (state == OFF) powerOn(); else powerOff();
  }
  if (state != OFF) {
    if (readButtonPress(Breset)) {
      Serial.println("RESET pressed");
      goToWait();
    }
    if (readButtonPress(Bmeasure)) {
      Serial.println("MEASURE pressed");
      if (state == WAIT || state == SHOW_RESULT) startMeasurement();
    }
  }

  if (state == MEASURING) {
    unsigned long now = millis();
    if (now - lastUpdate >= UPDATE_INTERVAL) {
      lastUpdate = now;
      simulateStep();
      showMeasuringScreen(currentBpm, now - measureStart);
    }
    if (millis() - measureStart >= MEASURE_DURATION) {
      endMeasurement();
    }
  }
  delay(5);
}

void powerOn() {
  state = WAIT;
  digitalWrite(PIN_LED, HIGH);
  showWaitScreen();
}
void powerOff() {
  state = OFF;
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  display.clearDisplay();
  display.display();
}
void goToWait() {
  state = WAIT;
  digitalWrite(PIN_LED, HIGH);
  digitalWrite(PIN_BUZZER, LOW);
  showWaitScreen();
}
void startMeasurement() {
  state = MEASURING;
  measureStart = millis();
  lastUpdate = 0;
  currentBpm = random(START_MIN, START_MAX + 1);
  showMeasuringScreen(currentBpm, 0);
}
void simulateStep() {
  int step = random(-3, 4);
  currentBpm += step;
  if (currentBpm < BPM_MIN) currentBpm = BPM_MIN;
  if (currentBpm > BPM_MAX) currentBpm = BPM_MAX;
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}
void endMeasurement() {
  state = SHOW_RESULT;
  finalBpm = currentBpm;
  showResultScreen(finalBpm);
  beep(800, 200);
  digitalWrite(PIN_LED, HIGH);
}
void showWaitScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,8);
  display.println("   may giam sat nhip tim");
  display.setTextSize(2);
  display.setCursor(0,28);
  display.println(" 0.1.1");
  display.setTextSize(1);
  display.setCursor(0,52);
  display.println("Nhan DO de bat/thu do");
  display.display();
}
void showMeasuringScreen(int bpm, unsigned long elapsed) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Dang do...");
  float progress = ((float)elapsed) / ((float)MEASURE_DURATION);
  if (progress > 1.0f) progress = 1.0f;
  int barWidth = (int)(progress * (SCREEN_WIDTH - 10));
  display.drawRect(4,12,SCREEN_WIDTH-8,10,SSD1306_WHITE);
  display.fillRect(5,13,barWidth,8,SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(0,28);
  char buf[8]; sprintf(buf,"%3d",bpm);
  display.print(buf);
  display.setTextSize(1);
  display.setCursor(88,36); display.println("bpm");
  display.display();
}
void showResultScreen(int bpm) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0); display.println("Ket qua do:");
  display.setTextSize(4); display.setCursor(0,12);
  char buf[8]; sprintf(buf,"%3d",bpm); display.print(buf);
  display.setTextSize(1); display.setCursor(96,28); display.println("bpm");
  display.setTextSize(1); display.setCursor(0,56);
  if (bpm < 60) display.println("Nhip cham (co the binh thuong)");
  else if (bpm <= 100) display.println("Nhip binh thuong");
  else display.println("Nhip nhanh (can khao sat)");
  display.display();
}
void beep(unsigned int freq, unsigned long duration_ms) {
  if (freq==0||duration_ms==0) return;
  unsigned long cycles = (unsigned long)((duration_ms*(unsigned long)freq)/1000UL);
  unsigned int halfPeriodUs = (unsigned int)(500000UL/freq);
  for (unsigned long i=0;i<cycles;i++){
    digitalWrite(PIN_BUZZER, HIGH); delayMicroseconds(halfPeriodUs);
    digitalWrite(PIN_BUZZER, LOW);  delayMicroseconds(halfPeriodUs);
  }
  digitalWrite(PIN_BUZZER, LOW);
}
/*
 * ════════════════════════════════════════════════════════════════
 * ADLS - Automated Drying Line System
 * ESP32 + Servo + LCD 1602 + Blynk + Mobile Hotspot
 * ════════════════════════════════════════════════════════════════
 */

#define BLYNK_TEMPLATE_ID "TMPL4lRnm3h9x"
#define BLYNK_TEMPLATE_NAME "ADLS System"
#define BLYNK_AUTH_TOKEN "nB1jjIJDv6LCsSEJHmhnd3vMD2CpJ_ct"
#define BLYNK_PRINT Serial

#include <WiFi.h>                // ESP32 WiFi
#include <BlynkSimpleEsp32.h>    // Blynk for ESP32
#include <ESP32Servo.h>          // Servo control
#include <Wire.h>                // I2C
#include <LiquidCrystal_I2C.h>   // LCD 1602 I2C

///////////////////////////////////////////////////////////////
// HOTSPOT WIFI (WPA2-PSK)
///////////////////////////////////////////////////////////////

char ssid[] = "Jose";      // Hotspot name
char pass[] = "yyyyyyyy";    // Hotspot password

///////////////////////////////////////////////////////////////
// LCD
///////////////////////////////////////////////////////////////

LiquidCrystal_I2C lcd(0x27, 16, 2);  // 16x2 LCD at address 0x27

///////////////////////////////////////////////////////////////
// PINS
///////////////////////////////////////////////////////////////

#define LIGHT_SENSOR_PIN 34
#define RAIN_SENSOR_PIN  32
#define SERVO_PIN        13

#define LED_GREEN  25
#define LED_YELLOW 4
#define LED_RED    15

#define BUTTON_MODE   16
#define BUTTON_TOGGLE 17

///////////////////////////////////////////////////////////////
// CONSTANTS
///////////////////////////////////////////////////////////////

#define SERVO_RETRACTED_ANGLE 0
#define SERVO_EXTENDED_ANGLE  90
#define SERVO_MOVE_DELAY      5

#define LIGHT_THRESHOLD_SUNNY 1000
#define LIGHT_THRESHOLD_DARK  1800

#define CHECK_INTERVAL 3000
#define DEBOUNCE_DELAY 50

///////////////////////////////////////////////////////////////
// STATE MACHINE
///////////////////////////////////////////////////////////////

enum LineState {
  LINE_RETRACTED,
  LINE_EXTENDED,
  LINE_EXTENDING,
  LINE_RETRACTING
};

Servo lineServo;
LineState currentState = LINE_RETRACTED;

bool autoMode = true;
bool servoMoving = false;

bool isSunny = false;
bool isRaining = false;

int lightLevel = 0;

unsigned long lastCheck = 0;

BlynkTimer timer;

///////////////////////////////////////////////////////////////
// FUNCTION PROTOTYPES (Prevents compile errors)
///////////////////////////////////////////////////////////////

void checkSensors();
void decideAction();
void extendLine();
void retractLine();
void updateLEDs();
void updateLCD();
void checkButtons();
void updateBlynkStatus();

///////////////////////////////////////////////////////////////
// SETUP
///////////////////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);

  // LCD init
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("ADLS Starting");

  // Pin modes
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_TOGGLE, INPUT_PULLUP);

  // Servo
  lineServo.attach(SERVO_PIN);
  lineServo.write(SERVO_RETRACTED_ANGLE);

  delay(2000);

  // Connect to WiFi + Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(2000L, updateBlynkStatus);

  lcd.clear();
  lcd.print("System Ready");
}

///////////////////////////////////////////////////////////////
// LOOP
///////////////////////////////////////////////////////////////

void loop() {

  Blynk.run();
  timer.run();

  checkButtons();

  if (millis() - lastCheck > CHECK_INTERVAL) {
    checkSensors();
    if (autoMode) decideAction();
    updateLEDs();
    updateLCD();
    lastCheck = millis();
  }
}

///////////////////////////////////////////////////////////////
// SENSOR LOGIC
///////////////////////////////////////////////////////////////

void checkSensors() {

  lightLevel = analogRead(LIGHT_SENSOR_PIN);
  isSunny = (lightLevel < LIGHT_THRESHOLD_SUNNY);

  int rainRead = digitalRead(RAIN_SENSOR_PIN);
  isRaining = (rainRead == LOW);

  Serial.print("Light: ");
  Serial.println(lightLevel);
  Serial.print("Rain: ");
  Serial.println(isRaining);
}

///////////////////////////////////////////////////////////////
// AUTO DECISION
///////////////////////////////////////////////////////////////

void decideAction() {

  if (isRaining) {
    retractLine();
    return;
  }

  if (isSunny && currentState == LINE_RETRACTED) {
    extendLine();
  }

  if (!isSunny && currentState == LINE_EXTENDED) {
    retractLine();
  }
}

///////////////////////////////////////////////////////////////
// SERVO CONTROL
///////////////////////////////////////////////////////////////

void extendLine() {

  if (currentState == LINE_EXTENDED) return;

  currentState = LINE_EXTENDING;
  servoMoving = true;

  for (int a = lineServo.read(); a <= SERVO_EXTENDED_ANGLE; a++) {
    lineServo.write(a);
    delay(SERVO_MOVE_DELAY);
  }

  currentState = LINE_EXTENDED;
  servoMoving = false;

  Blynk.virtualWrite(V3, 1);
}

void retractLine() {

  if (currentState == LINE_RETRACTED) return;

  currentState = LINE_RETRACTING;
  servoMoving = true;

  for (int a = lineServo.read(); a >= SERVO_RETRACTED_ANGLE; a--) {
    lineServo.write(a);
    delay(SERVO_MOVE_DELAY);
  }

  currentState = LINE_RETRACTED;
  servoMoving = false;

  Blynk.virtualWrite(V3, 0);
}

///////////////////////////////////////////////////////////////
// BUTTONS
///////////////////////////////////////////////////////////////

void checkButtons() {

  if (digitalRead(BUTTON_MODE) == LOW) {
    delay(200);
    autoMode = !autoMode;
    Blynk.virtualWrite(V4, autoMode ? 1 : 0);
  }

  if (!autoMode && digitalRead(BUTTON_TOGGLE) == LOW) {
    delay(200);
    if (currentState == LINE_RETRACTED) extendLine();
    else retractLine();
  }
}

///////////////////////////////////////////////////////////////
// LED STATUS
///////////////////////////////////////////////////////////////

void updateLEDs() {

  digitalWrite(LED_YELLOW, autoMode);
  digitalWrite(LED_RED, isRaining);
  digitalWrite(LED_GREEN, currentState == LINE_EXTENDED);
}

///////////////////////////////////////////////////////////////
// LCD UPDATE
///////////////////////////////////////////////////////////////

void updateLCD() {

  lcd.clear();
  lcd.setCursor(0,0);

  if (currentState == LINE_EXTENDED)
    lcd.print("Extended 90deg");
  else
    lcd.print("Retracted 0deg");

  lcd.setCursor(0,1);

  if (isRaining)
    lcd.print("RAIN!");
  else if (isSunny)
    lcd.print("Sunny");
  else
    lcd.print("Cloudy");
}

///////////////////////////////////////////////////////////////
// BLYNK
///////////////////////////////////////////////////////////////

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V3, V4);
}

void updateBlynkStatus() {
  Blynk.virtualWrite(V0, lightLevel);
  Blynk.virtualWrite(V1, isRaining ? 1 : 0);
  Blynk.virtualWrite(V4, autoMode ? 1 : 0);
}
/*
 * ════════════════════════════════════════════════════════════════
 *           ADLS - AUTOMATED DRYING LINE SYSTEM
 *           Servo Motor + Enterprise WiFi + LCD 1602
 * ════════════════════════════════════════════════════════════════
 *  ✓ Fully Synchronized Physical + Dashboard Control
 *  ✓ LCD 1602 I2C Real-time Status Display
 *  ✓ Sensor Fault Detection with System Shutdown
 *  ✓ Mode Switching with LED Confirmation (Yellow=AUTO, Red=MANUAL)
 *  ✓ Green LED for Extended Status
 *  ✓ Servo Limited: 0° – 90°
 * ════════════════════════════════════════════════════════════════
 */

#define BLYNK_TEMPLATE_ID "*************"
#define BLYNK_TEMPLATE_NAME "ADLS System"
#define BLYNK_AUTH_TOKEN "*********************************"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// IMPORTANT: Install LiquidCrystal_I2C library first!
// Arduino IDE: Sketch → Include Library → Manage Libraries
// Search: "LiquidCrystal_I2C" by Frank de Brabander



///////////////////////////////////////////////////////////////
// LCD 1602 I2C INITIALIZATION
///////////////////////////////////////////////////////////////

LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16x2 display

///////////////////////////////////////////////////////////////
// ENTERPRISE WIFI
///////////////////////////////////////////////////////////////

#define EAP_IDENTITY "******************************************"
#define EAP_PASSWORD "***********************"
const char* ssid = "WLsjgsjD";

///////////////////////////////////////////////////////////////
// PIN DEFINITIONS
///////////////////////////////////////////////////////////////

#define LIGHT_SENSOR_PIN 34
#define RAIN_SENSOR_PIN  32
#define SERVO_PIN        13

#define LED_GREEN  25   // Extended status indicator
#define LED_YELLOW 4    // AUTO mode indicator (blinks on toggle)
#define LED_RED    15   // MANUAL mode indicator (blinks on toggle) / Rain/Error

#define BUTTON_MODE   16   // Green button → Toggle AUTO/MANUAL mode (syncs with V4)
#define BUTTON_TOGGLE 17   // Red button → Extend/Retract in manual (syncs with V3)

///////////////////////////////////////////////////////////////
// SYSTEM CONSTANTS
///////////////////////////////////////////////////////////////

#define SERVO_RETRACTED_ANGLE 0
#define SERVO_EXTENDED_ANGLE  90   // LIMITED TO 90°
#define SERVO_MOVE_DELAY      5

#define CHECK_INTERVAL   3000
#define DEBOUNCE_DELAY   50
#define SENSOR_TIMEOUT   5000

#define LIGHT_THRESHOLD_SUNNY 1000
#define LIGHT_THRESHOLD_DARK  1800

#define ERROR_LIGHT_SENSOR 0x01
#define ERROR_RAIN_SENSOR  0x02

///////////////////////////////////////////////////////////////
// STATE MACHINE
///////////////////////////////////////////////////////////////

enum LineState {
  LINE_RETRACTED,
  LINE_EXTENDED,
  LINE_EXTENDING,
  LINE_RETRACTING,
  LINE_STOPPED
};

Servo lineServo;
LineState currentState = LINE_RETRACTED;

int systemError = 0;
bool autoMode = true;
bool servoMoving = false;
bool modeButtonProcessed = false;
bool toggleButtonProcessed = false;

bool isSunny = false;
bool isRaining = false;
bool lastRainState = false;

int lightLevel = 0;
unsigned long lastLightRead = 0;
unsigned long lastRainRead  = 0;
unsigned long lastSensorCheck = 0;

bool lastModeState = HIGH;
bool lastToggleState = HIGH;
unsigned long debounceMode = 0;
unsigned long debounceToggle = 0;

// LCD Update tracking
unsigned long lastLCDUpdate = 0;
#define LCD_UPDATE_INTERVAL 500  // Update LCD every 500ms

// LED blink tracking for mode changes
unsigned long modeChangeTime = 0;
bool isBlinkingMode = false;
#define BLINK_DURATION 600  // Duration of blink sequence (ms)

BlynkTimer timer;

///////////////////////////////////////////////////////////////
// SETUP
///////////////////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  ADLS - Automated Drying Line System      ║");
  Serial.println("║  Servo + WiFi + LCD 1602 Integration      ║");
  Serial.println("╚════════════════════════════════════════════╝\n");

  // ─── Initialize I2C for LCD ───
  Wire.begin(21, 22);  // SDA=GPIO21, SCL=GPIO22
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Startup message
  lcd.setCursor(0, 0);
  lcd.print("ADLS System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  Serial.println("✓ LCD 1602 Initialized (I2C: 0x27)");

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_TOGGLE, INPUT_PULLUP);

  // Turn off LEDs initially
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  lineServo.attach(SERVO_PIN);
  lineServo.write(SERVO_RETRACTED_ANGLE);

  Serial.println("✓ Hardware Initialized");
  Serial.println("✓ Servo at 0° (Retracted)\n");

  delay(2000);
  connectEnterpriseWiFi();

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  timer.setInterval(2000L, updateBlynkStatus);

  // Initial sensor read
  checkSensors();
  
  lcd.clear();
  updateLCDDisplay();

  Serial.println("SYSTEM READY (AUTO MODE ACTIVE)\n");
}

///////////////////////////////////////////////////////////////
// MAIN LOOP
///////////////////////////////////////////////////////////////

void loop() {

  Blynk.run();
  timer.run();

  checkButtons();

  if (millis() - lastSensorCheck >= CHECK_INTERVAL) {

    checkSensors();

    // Only execute auto decision if no sensor errors
    if (systemError == 0 && autoMode) {
      decideAction();
    }

    updateLEDs();
    lastSensorCheck = millis();
  }

  // Update LCD periodically
  if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    updateLCDDisplay();
    lastLCDUpdate = millis();
  }
}

///////////////////////////////////////////////////////////////
// WIFI CONNECTION
///////////////////////////////////////////////////////////////

void connectEnterpriseWiFi() {

  Serial.println("Connecting to WPA2-Enterprise WiFi...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  esp_eap_client_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  esp_eap_client_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  esp_eap_client_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
  esp_wifi_sta_enterprise_enable();

  WiFi.begin(ssid);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println("\n✓ WiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

///////////////////////////////////////////////////////////////
// SENSOR READING + ERROR DETECTION
///////////////////////////////////////////////////////////////

void checkSensors() {

  // Read light sensor
  lightLevel = analogRead(LIGHT_SENSOR_PIN);
  lastLightRead = millis();
  isSunny = (lightLevel < LIGHT_THRESHOLD_SUNNY);

  // Read rain sensor
  int rainReading = digitalRead(RAIN_SENSOR_PIN);
  isRaining = (rainReading == LOW);
  lastRainRead = millis();

  // Check sensor health
  checkSensorHealth();
  
  // If sensor error exists, system is paused
  if (systemError != 0) {
    displaySensorError();
    servoMoving = false;
    return;  // Skip auto logic if sensor error
  }

  clearSensorErrors();

  // Rain detection alert
  if (isRaining && !lastRainState) {
    Serial.println("\n🌧️  RAIN ALERT! Immediate retraction.");
    Blynk.logEvent("rain_detected");
  }

  lastRainState = isRaining;

  // Print sensor status to serial
  Serial.println("┌── SENSOR STATUS ─────────────┐");
  Serial.print("│ Light: ");
  Serial.print(lightLevel);
  Serial.print(isSunny ? " ☀️  SUNNY" :
              (lightLevel > LIGHT_THRESHOLD_DARK ? " 🌙 DARK" : " ☁️  CLOUDY"));
  Serial.println(" │");

  Serial.print("│ Rain: ");
  Serial.print(isRaining ? "🌧️  RAINING" : "☁️  DRY");
  Serial.println(" │");
  Serial.println("└───────────────────────────────┘\n");
}

///////////////////////////////////////////////////////////////
// SENSOR HEALTH CHECK + ERROR HANDLING
///////////////////////////////////////////////////////////////

void checkSensorHealth() {

  unsigned long now = millis();
  int previousError = systemError;

  // Check light sensor timeout
  if (now - lastLightRead > SENSOR_TIMEOUT) {
    systemError |= ERROR_LIGHT_SENSOR;
  }

  // Check rain sensor timeout
  if (now - lastRainRead > SENSOR_TIMEOUT) {
    systemError |= ERROR_RAIN_SENSOR;
  }

  // Alert when error detected
  if (systemError != 0 && previousError == 0) {
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  🚨 SENSOR FAULT DETECTED - SYSTEM PAUSED ║");
    Serial.println("║  Please check sensors and replace if faulty║");
    Serial.println("╚════════════════════════════════════════════╝\n");
    Blynk.logEvent("sensor_fault");
  }
}

void clearSensorErrors() {

  if (millis() - lastLightRead < SENSOR_TIMEOUT)
    systemError &= ~ERROR_LIGHT_SENSOR;

  if (millis() - lastRainRead < SENSOR_TIMEOUT)
    systemError &= ~ERROR_RAIN_SENSOR;

  if (systemError == 0) {
    Serial.println("✓ All sensors recovered - System resuming\n");
  }
}

void displaySensorError() {
  
  static unsigned long lastErrorDisplay = 0;
  
  if (millis() - lastErrorDisplay > 2000) {
    Serial.print("⚠️  SENSOR ERROR CODE: 0x");
    Serial.println(systemError, HEX);
    Serial.println("   System PAUSED - Waiting for sensor recovery...\n");
    lastErrorDisplay = millis();
  }
}

///////////////////////////////////////////////////////////////
// AUTO DECISION LOGIC
///////////////////////////////////////////////////////////////

void decideAction() {

  // PRIORITY 1: Rain detected
  if (isRaining) {
    if (currentState == LINE_EXTENDED || currentState == LINE_EXTENDING)
      retractLine();
    return;
  }

  // PRIORITY 2: Sunny conditions
  if (isSunny && currentState == LINE_RETRACTED) {
    extendLine();
  }
  // PRIORITY 3: Dark conditions
  else if (lightLevel > LIGHT_THRESHOLD_DARK &&
           (currentState == LINE_EXTENDED || currentState == LINE_EXTENDING)) {
    Blynk.logEvent("darkness_detected");
    retractLine();
  }
}

///////////////////////////////////////////////////////////////
// SERVO CONTROL (0–90°)
///////////////////////////////////////////////////////////////

void extendLine() {

  if (currentState == LINE_EXTENDING || currentState == LINE_EXTENDED)
    return;

  Serial.println("→ EXTENDING LINE (0° → 90°)");
  currentState = LINE_EXTENDING;
  servoMoving = true;

  for (int a = lineServo.read(); a <= SERVO_EXTENDED_ANGLE; a++) {
    lineServo.write(a);
    delay(SERVO_MOVE_DELAY);
    updateLEDs();
  }

  currentState = LINE_EXTENDED;
  servoMoving = false;

  Serial.println("✓ LINE EXTENDED at 90°\n");
  Blynk.logEvent("line_extended");
  
  // Sync dashboard
  Blynk.virtualWrite(V3, 1);   // Slider to extended position
  
  updateLCDDisplay();
}

void retractLine() {

  if (currentState == LINE_RETRACTING || currentState == LINE_RETRACTED)
    return;

  Serial.println("← RETRACTING LINE (90° → 0°)");
  currentState = LINE_RETRACTING;
  servoMoving = true;

  for (int a = lineServo.read(); a >= SERVO_RETRACTED_ANGLE; a--) {
    lineServo.write(a);
    delay(SERVO_MOVE_DELAY);
    updateLEDs();
  }

  currentState = LINE_RETRACTED;
  servoMoving = false;

  Serial.println("✓ LINE RETRACTED at 0°\n");
  Blynk.logEvent("line_retracted");
  
  // Sync dashboard
  Blynk.virtualWrite(V3, 0);   // Slider to retracted position
  
  updateLCDDisplay();
}

///////////////////////////////////////////////////////////////
// PHYSICAL BUTTON CONTROL
///////////////////////////////////////////////////////////////

void checkButtons() {

  bool modeState = digitalRead(BUTTON_MODE);
  bool toggleState = digitalRead(BUTTON_TOGGLE);

  // ─── MODE BUTTON (GREEN - GPIO 16) ───
  // Toggles between AUTO and MANUAL mode
  // Syncs with V4 (Mode Switch) on dashboard
  
  if (modeState != lastModeState) {
    debounceMode = millis();
  }

  if ((millis() - debounceMode) > DEBOUNCE_DELAY && modeState == LOW && !modeButtonProcessed) {
    toggleMode();
    modeButtonProcessed = true;
  }

  if (modeState == HIGH) {
    modeButtonProcessed = false;
  }

  lastModeState = modeState;

  // ─── TOGGLE BUTTON (RED - GPIO 17) ───
  // In MANUAL mode: extends/retracts line alternately
  // Syncs with V3 (Manual Slider) on dashboard
  
  if (toggleState != lastToggleState) {
    debounceToggle = millis();
  }

  if ((millis() - debounceToggle) > DEBOUNCE_DELAY && toggleState == LOW && !toggleButtonProcessed) {
    manualToggle();
    toggleButtonProcessed = true;
  }

  if (toggleState == HIGH) {
    toggleButtonProcessed = false;
  }

  lastToggleState = toggleState;
}

void toggleMode() {

  autoMode = !autoMode;
  modeChangeTime = millis();
  isBlinkingMode = true;

  Serial.println(autoMode ?
    "\n🟡 MODE CHANGED: AUTO (Yellow LED blinking)" :
    "\n🔴 MODE CHANGED: MANUAL (Red LED blinking)");

  // Sync with dashboard
  Blynk.virtualWrite(V4, autoMode ? 1 : 0);  // V4 = Mode switch
  Blynk.virtualWrite(V6, autoMode ? "AUTO MODE" : "MANUAL MODE");
  
  updateLCDDisplay();
}

void manualToggle() {

  Serial.println("\n🔴 Manual Toggle Button Pressed");

  if (!autoMode) {

    // In manual mode: toggle between extend and retract
    if (currentState == LINE_RETRACTED || currentState == LINE_RETRACTING) {
      extendLine();
    } else {
      retractLine();
    }
  }
  else {
    Serial.println("   ✗ Ignored - System in AUTO mode");
  }
}

///////////////////////////////////////////////////////////////
// DASHBOARD CALLBACKS (BLYNK)
///////////////////////////////////////////////////////////////

/**
 * V4 - AUTO/MANUAL Mode Switch (syncs with physical button)
 * 0 = MANUAL, 1 = AUTO
 */
BLYNK_WRITE(V4) {

  bool newMode = param.asInt();
  
  if (newMode != autoMode) {
    autoMode = newMode;
    modeChangeTime = millis();
    isBlinkingMode = true;

    Serial.println(autoMode ?
      "\n🟡 DASHBOARD: AUTO MODE (Yellow LED blinking)" :
      "\n🔴 DASHBOARD: MANUAL MODE (Red LED blinking)");

    Blynk.virtualWrite(V6, autoMode ? "AUTO MODE" : "MANUAL MODE");
    
    updateLCDDisplay();
  }
}

/**
 * V3 - Manual Control Slider / Extend-Retract (syncs with physical button)
 * 0 = RETRACT, 1 = EXTEND
 * Only works in MANUAL mode
 */
BLYNK_WRITE(V3) {

  if (!autoMode) {

    bool extendCommand = param.asInt();

    if (extendCommand == 1) {
      if (currentState != LINE_EXTENDED && currentState != LINE_EXTENDING) {
        extendLine();
      }
    } else {
      if (currentState != LINE_RETRACTED && currentState != LINE_RETRACTING) {
        retractLine();
      }
    }
  }
}

/**
 * V5 - Emergency Stop Button
 */
BLYNK_WRITE(V5) {

  if (param.asInt() == 1) {

    Serial.println("\n🛑 DASHBOARD EMERGENCY STOP");

    currentState = LINE_STOPPED;
    servoMoving = false;

    Blynk.virtualWrite(V4, 0);  // Switch to MANUAL
    autoMode = false;

    Blynk.virtualWrite(V6, "EMERGENCY STOP");
    Blynk.virtualWrite(V5, 0);  // Reset button
    
    updateLCDDisplay();
  }
}

/**
 * V2 - LCD Display Sync (Read-only)
 * Auto-updated by updateBlynkStatus()
 */

///////////////////////////////////////////////////////////////
// LED STATUS UPDATES
///////////////////////////////////////////////////////////////

void updateLEDs() {

  // If sensor error: Flash RED LED
  if (systemError != 0) {
    digitalWrite(LED_RED, millis() % 400 < 200);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_GREEN, LOW);
    return;
  }

  // Mode change blink sequence
  if (isBlinkingMode && (millis() - modeChangeTime) < BLINK_DURATION) {
    
    if (autoMode) {
      // AUTO mode: Blink YELLOW LED
      digitalWrite(LED_YELLOW, (millis() - modeChangeTime) % 200 < 100);
      digitalWrite(LED_RED, LOW);
    } else {
      // MANUAL mode: Blink RED LED
      digitalWrite(LED_RED, (millis() - modeChangeTime) % 200 < 100);
      digitalWrite(LED_YELLOW, LOW);
    }
  } else {
    // Normal operation (not blinking)
    isBlinkingMode = false;
    
    // YELLOW LED = AUTO mode indicator (steady)
    digitalWrite(LED_YELLOW, autoMode ? HIGH : LOW);
    
    // RED LED = Rain alert or error
    digitalWrite(LED_RED, isRaining ? HIGH : LOW);
    
    // GREEN LED = Line extended status
    digitalWrite(LED_GREEN, (currentState == LINE_EXTENDED) ? HIGH : LOW);
  }

  // YELLOW LED = Servo moving (during motion)
  if (servoMoving) {
    digitalWrite(LED_YELLOW, HIGH);
  }
}

///////////////////////////////////////////////////////////////
// LCD 1602 DISPLAY UPDATE
///////////////////////////////////////////////////////////////

void updateLCDDisplay() {

  lcd.clear();

  // ─── Line 1: Status ───
  lcd.setCursor(0, 0);
  
  if (systemError != 0) {
    // Sensor error display
    lcd.print("ERROR: 0x");
    lcd.print(systemError, HEX);
    lcd.print(" PAUSE");
  } else if (servoMoving) {
    // Moving status
    if (currentState == LINE_EXTENDING) {
      lcd.print("EXTENDING >>");
    } else if (currentState == LINE_RETRACTING) {
      lcd.print("RETRACTING <<");
    }
  } else {
    // Stationary status
    if (currentState == LINE_EXTENDED) {
      lcd.print("Extended 90deg");
    } else if (currentState == LINE_RETRACTED) {
      lcd.print("Retracted 0deg");
    } else if (currentState == LINE_STOPPED) {
      lcd.print("STOPPED");
    }
  }

  // ─── Line 2: Mode + Conditions ───
  lcd.setCursor(0, 1);
  
  if (systemError != 0) {
    // Error message on line 2
    if (systemError & ERROR_LIGHT_SENSOR) {
      lcd.print("Light Sensor BAD");
    } else if (systemError & ERROR_RAIN_SENSOR) {
      lcd.print("Rain Sensor BAD");
    }
  } else if (isRaining) {
    // Rain alert
    lcd.print("RAIN DETECTED!");
  } else if (isSunny) {
    // Sunny condition
    lcd.print("Sunny ");
    lcd.print(autoMode ? "AUTO" : "MANU");
  } else if (lightLevel > LIGHT_THRESHOLD_DARK) {
    // Dark condition
    lcd.print("Dark ");
    lcd.print(autoMode ? "AUTO" : "MANU");
  } else {
    // Cloudy condition
    lcd.print("Cloudy ");
    lcd.print(autoMode ? "AUTO" : "MANU");
  }
}

///////////////////////////////////////////////////////////////
// BLYNK CONNECTION HANDLERS
///////////////////////////////////////////////////////////////

BLYNK_CONNECTED() {
  Serial.println("✓ Blynk Connected!");
  Blynk.syncVirtual(V3, V4, V5);
  updateBlynkStatus();
}

void updateBlynkStatus() {

  Blynk.virtualWrite(V0, lightLevel);
  Blynk.virtualWrite(V1, isRaining ? 1 : 0);
  Blynk.virtualWrite(V4, autoMode ? 1 : 0);

  // V2 - LINE STATUS (LCD LINE 1 synced)
  String lineStatus;
  if (systemError != 0) {
    lineStatus = "ERROR: 0x";
    lineStatus += String(systemError, HEX);
  } else if (servoMoving) {
    if (currentState == LINE_EXTENDING) {
      lineStatus = "EXTENDING >>";
    } else if (currentState == LINE_RETRACTING) {
      lineStatus = "RETRACTING <<";
    }
  } else {
    if (currentState == LINE_EXTENDED) {
      lineStatus = "Extended 90deg";
    } else if (currentState == LINE_RETRACTED) {
      lineStatus = "Retracted 0deg";
    } else if (currentState == LINE_STOPPED) {
      lineStatus = "STOPPED";
    }
  }
  Blynk.virtualWrite(V2, lineStatus);

  // V5 - ADDITIONAL STATUS (LCD LINE 2 synced)
  String conditionStatus;
  if (systemError != 0) {
    if (systemError & ERROR_LIGHT_SENSOR) {
      conditionStatus = "Light Sensor BAD";
    } else if (systemError & ERROR_RAIN_SENSOR) {
      conditionStatus = "Rain Sensor BAD";
    }
  } else if (isRaining) {
    conditionStatus = "RAIN DETECTED!";
  } else if (isSunny) {
    conditionStatus = "Sunny ";
    conditionStatus += autoMode ? "AUTO" : "MANU";
  } else if (lightLevel > LIGHT_THRESHOLD_DARK) {
    conditionStatus = "Dark ";
    conditionStatus += autoMode ? "AUTO" : "MANU";
  } else {
    conditionStatus = "Cloudy ";
    conditionStatus += autoMode ? "AUTO" : "MANU";
  }
  Blynk.virtualWrite(V5, conditionStatus);

  // V6 - MODE STATUS
  Blynk.virtualWrite(V6, autoMode ? "AUTO MODE" : "MANUAL MODE");
}

// ════════════════════════════════════════════════════════════════
// END OF CODE
// ════════════════════════════════════════════════════════════════


## System Overview\
\
The ADLS operates as a finite-state machine controlling a servo-driven drying line:\
\
- AUTO Mode\
  - Extends line when sunny\
  - Retracts line when dark or raining\
- MANUAL Mode\
  - User-controlled via physical button or Blynk dashboard\
- Fail-safe Behavior\
  - Immediate retraction on rain detection\
  - LED-based state indication\
- Cloud Integration\
  - Real-time monitoring and control via Blynk\
\
---\
\
## Hardware Requirements\
\
- ESP32 development board  \
- Servo motor (0\'9690\'b0 mechanical range)  \
- Rain sensor module  \
- Light sensor (analog LDR or equivalent)  \
- LCD 1602 with I2C interface (0x27)  \
- Push buttons (2x)  \
- LEDs (Green, Yellow, Red)  \
- External power supply for servo (recommended)\
\
---\
\
## Pin Configuration\
\
Light Sensor     \uc0\u8594  GPIO 34  \
Rain Sensor      \uc0\u8594  GPIO 32  \
Servo            \uc0\u8594  GPIO 13  \
LED Green        \uc0\u8594  GPIO 25  \
LED Yellow       \uc0\u8594  GPIO 4  \
LED Red          \uc0\u8594  GPIO 15  \
Mode Button      \uc0\u8594  GPIO 16  \
Toggle Button    \uc0\u8594  GPIO 17  \
LCD SDA          \uc0\u8594  GPIO 21  \
LCD SCL          \uc0\u8594  GPIO 22  \
\
---\
\
## Software Stack\
\
- Arduino IDE (ESP32 board package)\
- WiFi (WPA2-PSK hotspot compatible)\
- Blynk IoT Cloud\
\
Libraries:\
- WiFi.h\
- BlynkSimpleEsp32.h\
- ESP32Servo.h\
- LiquidCrystal_I2C.h\
- Wire.h\
\
---\
\
## Network Configuration\
\
The system connects to a standard WPA2 mobile hotspot:\
\
char ssid[] = "Joseph";\
char pass[] = "yyyyyyyy";\
\
Connection handled via:\
\
Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);\
\
Ensure hotspot operates at 2.4 GHz, as ESP32 does not support 5 GHz networks.\
\
---\
\
## Operating Logic\
\
Environmental Decision Rules:\
\
1. Rain detected \uc0\u8594  Immediate retract\
2. Sunny and retracted \uc0\u8594  Extend\
3. Dark and extended \uc0\u8594  Retract\
\
State Machine:\
\
RETRACTED\
   \uc0\u8595 \
EXTENDING\
   \uc0\u8595 \
EXTENDED\
   \uc0\u8595 \
RETRACTING\
   \uc0\u8595 \
RETRACTED\
\
Servo motion is incremental to ensure mechanical stability.\
\
---\
\
## LED Indicators\
\
Yellow \uc0\u8594  AUTO mode active  \
Red    \uc0\u8594  Rain detected  \
Green  \uc0\u8594  Line fully extended  \
\
---\
\
## LCD Display\
\
The LCD provides:\
- Line position (Extended 90\'b0 / Retracted 0\'b0)\
- Environmental condition (Sunny / Cloudy / Rain)\
- System readiness\
\
---\
\
## Blynk Dashboard Integration\
\
Virtual Pins:\
\
V0 \uc0\u8594  Light sensor value  \
V1 \uc0\u8594  Rain status  \
V3 \uc0\u8594  Line position (0/1)  \
V4 \uc0\u8594  AUTO/MANUAL mode  \
\
Dashboard allows:\
- Remote manual override\
- Mode switching\
- Real-time telemetry monitoring\
\
---\
\
## Installation\
\
1. Install ESP32 board support in Arduino IDE.\
2. Install required libraries via Library Manager.\
3. Configure Blynk Template ID and Auth Token.\
4. Set hotspot SSID and password.\
5. Upload firmware to ESP32.\
\
---\
\
## Safety & Reliability Notes\
\
- Use external power for servo to prevent ESP32 brownouts.\
- Add mechanical limit stops if used in real deployment.\
- Consider adding hysteresis for light threshold in outdoor installations.\
- Protect electronics from moisture exposure.\
\
---\
\
## Project Purpose\
\
This project demonstrates:\
- Embedded systems design\
- IoT cloud synchronization\
- Finite-state machine implementation\
- Real-world automation control logic\
- Sensor-driven actuation systems\
\
Suitable for academic demonstrations, IoT prototyping, and smart home automation experimentation.}

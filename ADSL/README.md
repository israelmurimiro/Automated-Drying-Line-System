# ADLS – Automated Drying Line System

The **Automated Drying Line System (ADLS)** is an IoT-enabled smart home solution designed to protect laundry from inclement weather. By leveraging real-time environmental sensing and automated mechanical actuation, the system ensures your drying line is extended during optimal sunlight conditions and proactively retracted when rain is detected.

---

## Project Overview

The ADLS functions as an intelligent **finite-state machine** powered by an ESP32. It intelligently monitors atmospheric conditions via LDR (light) and rain sensors, providing a hands-free experience while maintaining user control through a physical interface and the **Blynk IoT cloud dashboard**.

### Key Features
* **Intelligent Automation:** Automatically extends/retracts based on environmental thresholds.
* **Fail-safe Protection:** Prioritizes rain detection to ensure immediate retraction regardless of current mode.
* **Dual-Control Modes:** Seamlessly switch between **AUTO** (sensor-driven) and **MANUAL** (user-driven) via push buttons or the mobile app.
* **Cloud Monitoring:** Real-time telemetry and remote control integration using the Blynk IoT platform.
* **Visual Feedback:** On-device status reporting via an I2C LCD and diagnostic LED indicators.

---

## Technical Specifications

### Hardware Requirements
* **Controller:** ESP32 Development Board
* **Actuation:** Servo Motor (0°–90°)
* **Sensing:** Rain Sensor Module, Light Sensor (LDR)
* **UI/UX:** LCD 1602 (I2C), Push Buttons, LEDs (Green, Yellow, Red)
* **Power:** External power supply (recommended for servo stability)

### Logic & Operational Flow
The system operates on a state-based logic loop to maintain mechanical and electrical safety:

1. **Rain Detection:** Triggered immediately; overrides all states to retract the line.
2. **Sunny Conditions:** Line extends if currently retracted.
3. **Low Light/Dark:** Line retracts if currently extended.

---

## Dashboard Integration (Blynk)

The system syncs status data to the cloud using the following virtual pins:

| Virtual Pin | Function | Description |
| :--- | :--- | :--- |
| **V0** | Light Value | Real-time ambient light intensity |
| **V1** | Rain Status | Boolean flag for precipitation |
| **V3** | Line Position | Current state (0 = Retracted, 1 = Extended) |
| **V4** | Mode Switch | Toggle between AUTO and MANUAL |

---

## Getting Started

1. **Dependencies:** Ensure the Arduino IDE is configured for ESP32 and install the following via Library Manager: `ESP32Servo`, `LiquidCrystal_I2C`, `Blynk`, and `Wire`.
2. **Connectivity:** The system utilizes a 2.4 GHz WiFi connection. Update the `ssid` and `pass` variables in the firmware to match your local hotspot.
3. **Deployment:** Configure your specific **Blynk Auth Token** within the source code before uploading.

> **Safety Note:** To ensure longevity and prevent brownouts, always use an external power source for the servo motor and ensure all sensitive electronics are housed in a water-resistant enclosure for outdoor use.

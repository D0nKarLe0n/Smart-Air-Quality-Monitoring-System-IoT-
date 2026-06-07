# Smart Air Quality Edge Node with Azure IoT Telemetry

An end-to-end Internet of Things (IoT) edge device built on the **ESP32-S3** microcontroller using the **ESP-IDF** framework (C/C++). This system performs real-time air quality monitoring, automated environmental control via PWM, and secure cloud telemetry using Microsoft Azure IoT Hub.

Developed with a focus on hardware-software integration, low-latency I2C communication, and reliable edge-to-cloud data pipelines.

---

## 🚀 Key Features

* **Real-Time Sensor Fusion:** Aggregates data from multiple environmental sensors (CO2, VOC, Temperature, Humidity) using optimized I2C and ADC polling.
* **Closed-Loop Actuation:** Implements automated, threshold-based PWM control of an external ventilation fan depending on real-time CO2 and VOC concentrations.
* **Secure Cloud Telemetry:** Utilizes MQTT over TLS/SSL (with embedded root certificates) to publish JSON-formatted telemetry to **Microsoft Azure IoT Hub**.
* **Edge Display:** Provides an immediate local readout via a 128x64 OLED display, utilizing a custom lightweight I2C driver and font library.
* **Fault-Tolerant Networking:** Includes auto-reconnect logic for Wi-Fi and NTP time synchronization before establishing cloud connections.

---

## 🛠️ Hardware Stack

* **Microcontroller:** ESP32-S3 (Xtensa Dual-Core, Wi-Fi)
* **CO2/Temp/Hum Sensor:** Sensirion SCD41 (Photoacoustic NDIR, I2C)
* **VOC Sensor:** MQ135 (Analog ADC)
* **Display:** 0.96" OLED SSD1306 (I2C)
* **Actuator:** 5V/12V DC Fan driven via LEDC (PWM)

### ⚠️ Hardware Engineering Notes & Troubleshooting
**SCD41 Power Supply & I2C Bus Stability:** During development, reading failures (`ESP_ERR_INVALID_STATE`) were observed on the I2C bus. Hardware diagnostics revealed that powering the SCD41 from the ESP32's 3.3V rail caused voltage drops during the sensor's peak photoacoustic measurement phase (~200mA impulse). This caused the sensor's internal timer to reset, rejecting I2C read requests. 

**Solution:** Rerouted the SCD41 VCC directly to the 5V (VIN) rail while maintaining 3.3V logic levels on SDA/SCL, and implemented a Data Ready Status check (`0xE4B8` register) before querying telemetry. This completely stabilized the bus.

---

## 💻 Software Stack

* **Framework:** ESP-IDF v5.x (FreeRTOS based)
* **Language:** C 
* **Cloud Platform:** Microsoft Azure IoT Hub
* **Protocols:** Wi-Fi, MQTT, I2C, NTP
* **Data Format:** cJSON

---

## 📊 System Architecture

```text
[ SCD41 Sensor ] --(I2C)--> |
[ MQ135 Sensor ] --(ADC)--> |--> [ ESP32-S3 MCU ] --(TLS/MQTT)--> [ Azure IoT Hub ]
[ OLED Display ] <-(I2C)--  |           |
                            |--(PWM)--> [ Cooling Fan ]
```

⚙️ Build and Run Instructions
1. Prerequisites
Ensure you have the ESP-IDF framework (v5.0+) installed and configured on your system.

2. Configure Secrets
To prevent exposing sensitive credentials, this repository uses a separate secrets file.

Copy the template file: cp main/secrets_example.h main/secrets.h

Update main/secrets.h with your Wi-Fi credentials and Azure connection strings:
```
#ifndef SECRETS_H
#define SECRETS_H

#define SECRET_WIFI_SSID "your_wifi_ssid"
#define SECRET_WIFI_PASS "your_wifi_password"

#define SECRET_AZURE_HOST "your-iot-hub.azure-devices.net"
#define SECRET_AZURE_DEVICE_ID "your_device_id"
#define SECRET_AZURE_PASSWORD "your_sas_token"

#endif
```

3. Flash and Monitor
Use the ESP-IDF tools to build, flash, and open the serial monitor:

Bash
```
idf.py build
idf.py -p (YOUR_PORT) flash monitor
```


📈 Sample Telemetry Output
JSON
{
  "co2": 527,
  "temperature": 22.4,
  "humidity": 45.1,
  "voc": 530,
  "fan_pwm": 0
}



https://github.com/user-attachments/assets/712cf153-1dc3-45b7-8482-522faaeb89f7


# SmartRide — ESP32 Smart Helmet & Vehicle Safety System

> A dual-node IoT safety system enforcing ignition interlocks, crash detection, and real-time GPS emergency alerts — all over a local ESP-NOW radio link with simultaneous WiFi internet access.

---

## System Overview

```
╔══════════════════════════════╗                    ╔══════════════════════════════════════╗
║        HELMET NODE           ║                    ║          VEHICLE GATEWAY             ║
║      (ESP32 / ESP32-CAM)     ║                    ║             (ESP32)                  ║
║                              ║                    ║                                      ║
║  ┌───────────────────────┐   ║                    ║  ┌─────────────┐  ┌───────────────┐  ║
║  │  MQ-3 Alcohol Sensor  │   ║   ESP-NOW (Local)  ║  │   MPU6050   │  │  NEO-6M GPS   │  ║
║  │  GPIO 34 (Digital)    │   ║  ══════════════►   ║  │  Crash Det. │  │  Live Coords  │  ║
║  └───────────────────────┘   ║                    ║  │  I2C 21/22  │  │  UART2 16/17  │  ║
║                              ║   Heartbeat Every  ║  └─────────────┘  └───────────────┘  ║
║  ┌───────────────────────┐   ║       1 sec        ║                                      ║
║  │  IR Proximity Sensor  │   ║  ◄══════════════   ║  ┌─────────────┐  ┌───────────────┐  ║
║  │  GPIO 35 (Digital)    │   ║                    ║  │ Relay Module│  │  SOS Button   │  ║
║  └───────────────────────┘   ║                    ║  │  Ignition   │  │  Manual Alert │  ║
║                              ║                    ║  │  GPIO 25    │  │  GPIO 15      │  ║
╚══════════════════════════════╝                    ║  └─────────────┘  └───────────────┘  ║
                                                    ║                                      ║
                                                    ║              │ WiFi                  ║
                                                    ║              ▼                       ║
                                                    ║  ┌────────────────────────────────┐  ║
                                                    ║  │   Telegram Bot  📱             │  ║
                                                    ║  │   GPS-tagged Emergency Alerts  │  ║
                                                    ║  └────────────────────────────────┘  ║
                                                    ╚══════════════════════════════════════╝
```

The **Helmet Node** continuously streams sensor data over ESP-NOW. The **Vehicle Gateway** processes this data locally to control the ignition relay, while simultaneously connected to WiFi for GPS-tagged emergency Telegram alerts.

---

## ✨ Features

| Feature | Details |
|---|---|
| 🔒 **Ignition Interlock** | Engine relay stays OFF unless the helmet is worn AND the rider is sober |
| 📡 **ESP-NOW + WiFi Coexistence** | High-speed local link with the helmet while connected to the internet |
| 💥 **Crash Detection** | MPU6050 detects hard impacts (>15 m/s²) and tip-overs (Z-axis < 3 m/s²) |
| 🌍 **Live GPS Tracking** | NEO-6M appends a Google Maps link to every emergency alert |
| 📱 **Telegram Alerts** | Instant messages to a designated chat for crashes or SOS button presses |
| 💓 **Heartbeat Failsafe** | If the helmet disconnects for >3 seconds, the engine is immediately cut |

---

## 🛠️ Hardware

### Node 1 — Helmet (Sensor Node)

| Component | Connection |
|---|---|
| ESP32 / ESP32-CAM | Microcontroller |
| MQ-3 Alcohol Sensor | GPIO 34 (Digital Input) |
| IR Proximity Sensor | GPIO 35 (Digital Input) |

### Node 2 — Vehicle (Gateway Node)

| Component | Connection |
|---|---|
| ESP32 | Microcontroller |
| NEO-6M GPS Module | UART2 — RX: GPIO 16, TX: GPIO 17 |
| MPU6050 Gyro/Accel | I2C — SDA: GPIO 21, SCL: GPIO 22 |
| Relay Module | GPIO 25 (Ignition Control) |
| Emergency Button | GPIO 15 (INPUT_PULLUP) |
| Onboard LED | GPIO 2 (Status Indicator) |

---

## 📦 Software Dependencies

Install the following via **Arduino Library Manager** (`Sketch → Include Library → Manage Libraries`):

| Library | Author |
|---|---|
| `UniversalTelegramBot` | Brian Lough |
| `TinyGPSPlus` | Mikal Hart |
| `Adafruit MPU6050` | Adafruit |
| `Adafruit Unified Sensor` | Adafruit |
| `ArduinoJson` | Benoit Blanchon |

> **Board support:** Install the **ESP32 board package** by Espressif via `File → Preferences → Additional Board Manager URLs`:
> `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

---

## ⚙️ Setup & Configuration

### Step 1 — Flash the Gateway (Vehicle) Node

1. Open `esp32bike.ino` in Arduino IDE.
2. Fill in your credentials:
   ```cpp
   #define BOT_TOKEN  "YOUR_TELEGRAM_BOT_TOKEN"
   #define CHAT_ID    "YOUR_TELEGRAM_CHAT_ID"
   const char* ssid     = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
3. Flash the board. Open Serial Monitor at **115200 baud**.
4. Note the **WiFi channel** printed on boot (e.g., `WiFi Channel: 11`).
5. Note the **MAC address** of this ESP32 (run `WiFi.macAddress()` or check the label).

### Step 2 — Flash the Helmet (Sensor) Node

1. Open `esp32helmet.ino` in Arduino IDE.
2. Paste the Gateway's MAC address and WiFi channel:
   ```cpp
   uint8_t mainBoardAddress[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // ← Replace
   #define WIFI_CHANNEL 11                                               // ← Replace
   ```
3. Flash the board. Confirm in Serial Monitor: `✅ ESP-NOW LINK OK`.

### Step 3 — Telegram Bot Setup

1. Open Telegram and search for **@BotFather**.
2. Run `/newbot` and follow the prompts to get your `BOT_TOKEN`.
3. Send a message to your new bot, then visit:
   `https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getUpdates`
4. Copy your `chat_id` from the JSON response.

---

## 🔒 Ignition Logic

The relay on the Gateway node is controlled by this truth table:

| MQ-3 (Alcohol) | IR (Helmet Worn) | Engine State |
|---|---|---|
| `0` (Sober) | `1` (Worn) | ✅ **ON** |
| `1` (Alcohol detected) | Any | ❌ **OFF** |
| Any | `0` (Not worn) | ❌ **OFF** |
| No ESP-NOW signal for >3s | — | ❌ **OFF** (Failsafe) |

---

## 🚨 Crash Detection Thresholds

Tunable in `esp32bike.ino`:

```cpp
// Hard impact: X or Y axis acceleration exceeds 15 m/s²
bool hardImpact = (abs(a.acceleration.x) > 15.0 || abs(a.acceleration.y) > 15.0);

// Tip-over: Z axis (normally ~9.8 m/s²) drops below 3 m/s²
bool bikeFallen = (abs(a.acceleration.z) < 3.0);
```

Alert cooldown is **15 seconds** to prevent spam.

---

## 📁 Repository Structure

```
SmartRide/
├── esp32bike.ino        # Gateway Node — Vehicle firmware
├── esp32helmet.ino      # Sensor Node — Helmet firmware
└── README.md
```

---

## 🛡️ Failsafe Summary

- **Helmet disconnects mid-ride** → Engine cuts off within 3 seconds.
- **WiFi drops** → ESP-NOW local link and ignition interlock remain fully operational.
- **GPS no lock** → Alert is still sent, noting satellites are still searching.
- **Crash detected** → Telegram alert fired with 15-second cooldown to prevent spam.

---

## 🤝 Contributing

Pull requests are welcome! Please open an issue first to discuss major changes. For hardware modifications, include updated wiring diagrams.

---

## 📄 License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.

---

> Built with ❤️ using ESP32, Arduino, and the ESP-NOW protocol.

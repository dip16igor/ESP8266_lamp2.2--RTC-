# ESP8266 Smart Lamp with RTC

[Русская версия (Russian version)](README_RU.md)

An ESP8266-based smart lamp with real-time clock functionality, supporting automatic scheduling, smooth brightness control, and Telegram bot integration.

## ⚙️ Features

- **Manual Control:**
  - Rotary encoder for smooth brightness adjustment
  - Button for quick on/off and mode switching
  - Gamma correction for natural brightness perception
- **Automatic Operation:**
  - RTC-based scheduling for automatic on/off
  - Gradual wake-up light (sunrise simulation)
  - Configurable schedules for different times of day
- **Telegram Integration:**
  - Remote control via Telegram bot
  - Status notifications
  - OTA (Over-The-Air) firmware updates
- **Visual Feedback:**
  - Dual NeoPixel LED indicators for status display
  - Different colors indicate various operating modes
- **Power Management:**
  - DC-DC converter control for efficient operation
  - Smooth power transitions

## 🔌 Hardware Components

- **ESP8266** microcontroller (e.g., NodeMCU v2)
- **Rotary Encoder** with button for brightness control
- **NeoPixel (WS2812B)** LED strip (2 LEDs) for status indication
- **DC-DC Converter** for LED power control
- **PWM Output** for brightness control

## 📚 Libraries Used

- `EncButton` - For rotary encoder handling
- `Adafruit_NeoPixel` - For LED control
- `FastBot` - For Telegram bot integration
- `TimeLib` and `TimeAlarms` - For RTC functionality
- `NTPClient` - For time synchronization

## 🛠️ Setup

1. Create `src/secrets.h` with your configuration:
```cpp
#pragma once

// WiFi Settings
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Telegram Bot Settings
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define ADMIN_ID "YOUR_CHAT_ID"
```

2. Adjust the time zones and schedules in `main.cpp`:
```cpp
#define OffsetTime 5 * 60 * 60 // UTC offset in seconds
```

## ⚡ Operation Modes

- **Manual Mode (`STATE_ON_MANUAL`):**
  - Direct brightness control via encoder
  - Full range from 0 to 100%

- **Auto On (`STATE_ON_AUTO`):**
  - Quick automatic brightness increase
  - Stops at preset brightness level

- **Slow Auto On (`STATE_ON_AUTO_SLOW`):**
  - Gradual brightness increase (sunrise simulation)
  - Used for morning wake-up

- **Auto Off (`STATE_OFF_AUTO`):**
  - Gradual dimming to off
  - Can be triggered by timer or command

## 🤖 Telegram Commands

- `/start` - Show available commands
- `/ping` - Get current status (uptime, WiFi signal)
- `/restart` - Restart the device

## 📱 Status Indication

LED colors indicate different states:
- Red - Off or Error
- Green - Manual operation
- Blue - Connecting/Setup
- Purple - Command received
- Cyan - Auto mode active

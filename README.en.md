# Automatic Potted Plant Irrigation System

[中文](./README.md)

This is an Arduino-based automatic irrigation project for potted plants. It monitors soil moisture, reports low water levels in the tank, protects the system under low-light conditions, and shows runtime status on an OLED display.

For hardware interface details, see the [interface documentation](./docs/interface-documentation.md).

The Wokwi reference provided in the original documentation is older, and some hardware parts may not match the actual build. Use it for reference only.

## Features

- Automatically reads soil moisture and starts the pump when the soil is too dry.
- Reads the water tank level and stops the pump when water is insufficient to avoid dry running.
- Uses the LED and active buzzer to provide intermittent low-water alerts.
- Stops irrigation under low-light conditions and suppresses low-water alerts in that state.
- Shows the current plant mode, system mode, light value, soil moisture value, and tank status on the OLED display.
- Uses Button 1 to switch plant modes and Button 2 to switch system modes.

## Hardware

- Arduino Uno or compatible development board
- Soil moisture sensor
- Light sensor
- Water level sensor
- 0.96-inch SSD1306 OLED display
- Active buzzer module
- Relay module
- 5V water pump
- LED and current-limiting resistor
- Two push buttons
- Breadboard and jumper wires

## Pin Connections

See the interface documentation for full wiring details.

| Arduino Pin | Peripheral              |
| ----------- | ----------------------- |
| D2          | Button 1                |
| D3          | Button 2                |
| D4          | Status LED              |
| D5          | Active buzzer           |
| D6          | Relay IN                |
| A0          | Light sensor AO         |
| A1          | Soil moisture sensor AO |
| A2          | Water level sensor S    |
| A4          | OLED SDA                |
| A5          | OLED SCL                |

The relay controls power to the water pump. In the program, `LOW` means the relay is engaged and the pump is running, while `HIGH` means the pump is off.

## Program Logic

The program continuously reads light, soil moisture, and water level data inside `loop()`. It then decides whether to irrigate, whether to trigger an alert, and what to show on the OLED display.

Sensor reading conventions:

- A higher soil moisture reading means the soil is drier.
- A higher light reading means the environment is darker.
- A lower water level reading means there is less water in the tank.

Core thresholds are defined near the top of `template.ino`:

| Constant                 | Purpose                                 |
| ------------------------ | --------------------------------------- |
| `LIGHT_NEED_IRRIGATE`    | Light threshold                         |
| `HUMIDITY_NEED_IRRIGATE` | Soil dryness threshold                  |
| `NO_WATER_LEVEL`         | Severe water shortage threshold         |
| `LOW_WATER_LEVEL`        | Low-water alert threshold               |
| `PUMP_WORK_TIME`         | Pump runtime for one irrigation cycle   |
| `IRRIGATE_COOLDOWN`      | Cooldown time between irrigation cycles |

## Button Controls

| Button   | Function           |
| -------- | ------------------ |
| Button 1 | Switch plant mode  |
| Button 2 | Switch system mode |

The default OLED font has limited support for Chinese text, so the program UI uses English labels for modes and status values.

## Usage

1. Install the required libraries in the Arduino IDE: `Adafruit GFX Library` and `Adafruit SSD1306`.
2. Wire the hardware according to the [interface documentation](./docs/interface-documentation.md).
3. Open `template.ino` and select Arduino Uno or a compatible board.
4. Compile and upload the program.
5. Adjust the threshold constants based on real sensor readings.

## Files

| File                                 | Description                              |
| ------------------------------------ | ---------------------------------------- |
| `SmallAutomaticIrrigationSystem.ino` | Main Arduino program                     |
| `docs/接口文档.md`                       | Hardware connection and interface notes  |
| `README.md`                          | Chinese project overview and usage guide |
| `README.en.md`                       | English project overview and usage guide |

## Notes

- Active buzzer modules may be triggered by either a high or low level. If the buzzer state is inverted, adjust `BUZ_ON` and `BUZ_OFF`.
- Use a separate power supply for the water pump when possible. The development board should only control the pump through the relay.
- When connecting real sensors for the first time, check the raw readings on the OLED before adjusting thresholds.

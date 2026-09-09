# Automotive Fuel-Vapor Monitor

An ESP32-based embedded-system prototype for detecting abnormal combustible-gas or fuel-vapor readings. The system uses an MQ-2 gas sensor, a simulated vehicle-speed input, adaptive baseline calibration, RGB status indication, and an audible alert.

> Project phase: Phase 1B - speed-indexed baseline calibration

## Project Objective

A fixed gas threshold can give unreliable results because sensor readings may vary with operating conditions. This project improves detection by storing clean-air baseline readings at different simulated vehicle speeds and calculating an expected baseline for the current speed.

The ESP32 compares the live gas reading with this dynamic baseline and identifies the condition as normal, warning, or alert.

## System Overview

```text
MQ-2 Gas Sensor ----\
                     \
Potentiometer --------> ESP32 ----> RGB LED
  (speed input)        |       \--> Buzzer
                       |
Push Button -----------/
```

## Main Features

- Detects combustible-gas or fuel-vapor readings with an MQ-2 sensor
- Simulates vehicle speed from 0 to 120 km/h using a potentiometer
- Captures gas-sensor calibration points at different speeds
- Stores calibration values in ESP32 flash memory
- Calculates a dynamic baseline by interpolating between calibration points
- Smooths sensor readings using a moving-average filter
- Uses alert hysteresis to prevent rapid alert switching
- Provides normal, warning, alert, warm-up, and calibration indications
- Supports buzzer mute/unmute with a short button press
- Starts calibration with a long button press

## Hardware Requirements

- ESP32 development board
- MQ-2 gas sensor module
- Potentiometer
- Common-cathode RGB LED
- Three current-limiting resistors for the RGB LED
- Buzzer
- Push button
- Breadboard and jumper wires

## Pin Connections

| Component | ESP32 pin | Description |
|---|---:|---|
| MQ-2 analog output | GPIO 34 | Reads the gas-sensor value |
| Potentiometer wiper/output | GPIO 35 | Reads the simulated speed |
| RGB LED red channel | GPIO 25 | Red alert indication |
| RGB LED green channel | GPIO 26 | Green normal indication |
| RGB LED blue channel | GPIO 27 | Blue warm-up/calibration indication |
| Buzzer signal pin | GPIO 14 | Audible alert output |
| Push button | GPIO 13 | Mute and calibration input |

Connect the push button between GPIO 13 and GND. The program uses the ESP32 internal pull-up resistor.

## Important Electrical Note

ESP32 analog-input pins must not receive more than 3.3 V.

Some MQ-2 modules operate at 5 V and can provide an analog output above 3.3 V. Check the sensor-module output before connecting it to GPIO 34. Use a suitable voltage divider or level-shifting circuit if needed.

## Operating States

| State | LED color | Buzzer | Meaning |
|---|---|---|---|
| Warm-up | Blue | Off | MQ-2 sensor is stabilizing for 30 seconds |
| Calibration | Blue | Off | A clean-air baseline is being captured |
| Normal | Green | Off | Gas reading is within the expected range |
| Warning | Yellow | Off | Gas reading is above the warning margin |
| Alert | Red | On unless muted | Gas reading is above the alert margin |
| No calibration | Green | Off | Calibration is required before dynamic detection |

## Controls

| Action | Function |
|---|---|
| Short press button | Mute or unmute the buzzer |
| Hold button for more than 2 seconds | Save a calibration point at the current speed |

During calibration, keep the MQ-2 sensor in clean air. The program samples the sensor for 3 seconds and stores the average reading.

## Calibration Method

1. Upload the program to the ESP32.
2. Wait for the 30-second sensor warm-up to finish.
3. Set the potentiometer to a speed position, such as 0 km/h.
4. Ensure the sensor is in clean air.
5. Hold the button for more than 2 seconds.
6. Repeat at several speed positions, such as 30, 60, 90, and 120 km/h.
7. The ESP32 saves each baseline reading in flash memory, so the values remain after a power cycle.

Up to 10 calibration points can be stored. A new calibration point near an existing speed replaces the old value.

## Detection Logic

The program calculates:

```text
delta = current gas reading - dynamic baseline
```

The dynamic baseline is calculated from the saved calibration points for the current simulated speed.

| Parameter | Value | Purpose |
|---|---:|---|
| Warm-up time | 30 seconds | Allows the MQ-2 sensor to stabilize |
| Calibration sampling time | 3 seconds | Averages readings during calibration |
| Moving-average samples | 5 | Reduces sensor noise |
| Warning margin | 200 | Starts the warning state |
| Alert margin | 400 | Enters the alert state |
| Clear margin | 150 | Clears an existing alert |

The separate alert and clear margins provide hysteresis. This keeps the system from repeatedly entering and leaving alert mode when the sensor value fluctuates near a threshold.

## Software Requirements

- Arduino IDE
- ESP32 board package for Arduino
- `Preferences.h` library, included with the ESP32 Arduino core

## Installation and Upload

1. Install the Arduino IDE.
2. Add ESP32 board support through the Arduino Board Manager.
3. Clone or download this repository.
4. Open `automotive_fuel_vapor_monitor/automotive_fuel_vapor_monitor.ino`.
5. Select the correct ESP32 board and serial port.
6. Upload the sketch.
7. Open Serial Monitor at `115200` baud to view readings, calibration messages, and system state.

## Serial Monitor Output

The Serial Monitor displays values similar to:

```text
Gas: 1450  Speed: 60  DynBaseline: 1120  Delta: 330  State: WARN
```

This shows the current gas reading, simulated speed, interpolated baseline, difference from the baseline, and active system state.

## Repository Structure

```text
automotive-fuel-vapor-monitor/
├── automotive_fuel_vapor_monitor/
│   └── automotive_fuel_vapor_monitor.ino
├── README.md
└── LICENSE
```

## Limitations and Safety

This project is an educational prototype. The MQ-2 sensor is not a certified automotive safety sensor, and this system must not be used as the only safety mechanism in a real vehicle.

For real automotive deployment, the design would require certified sensors, environmental testing, enclosure design, power protection, fault detection, and compliance with applicable automotive standards.

## Future Improvements

- Add an OLED or LCD display for live values
- Record sensor data to an SD card or cloud dashboard
- Add GPS-based location reporting
- Use a real vehicle-speed input instead of a potentiometer
- Add wireless alerts through Wi-Fi or Bluetooth
- Add a mobile application for monitoring and notifications

## Author

Ananya Shreyasee

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

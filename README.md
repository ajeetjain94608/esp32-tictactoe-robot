# ESP32-CAM Tic-Tac-Toe Robot

An autonomous 5x5 Tic-Tac-Toe playing robot powered by ESP32-CAM with computer vision, robotic arm, and TFT display.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--CAM-green.svg)

## Features

- **5x5 Tic-Tac-Toe Game** - Extended board for strategic gameplay
- **Computer Vision** - Real-time piece detection (Red = Human, Blue = Robot)
- **Robotic Arm** - 3-servo arm with electromagnetic gripper
- **TFT Display** - 240x320 graphical game interface
- **Anti-Cheat System** - Detects and prevents invalid moves
- **WiFi Configuration** - Easy setup via captive portal
- **Web Calibration** - Browser-based servo and grid calibration
- **Score Tracking** - Persistent win/loss tracking

## Project Structure

```
esp32-tictactoe-robot/
├── firmware/
│   └── esp32_tictactoe_robot/    # Arduino sketch
│       ├── esp32_tictactoe_robot.ino
│       ├── camera_pins.h
│       ├── hardware_config.h
│       ├── image_processing*.c/h
│       ├── serial_utils.c/h
│       ├── web_page.h
│       └── web_styles.h
├── hardware/
│   └── schematics/               # Circuit diagrams
├── docs/
│   ├── SETUP.md                  # Setup guide
│   └── images/                   # Documentation images
├── README.md
└── LICENSE
```

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32-CAM | AI-Thinker module with OV2640 |
| TFT Display | ST7789V 240x320 |
| PCA9685 | 16-channel PWM servo driver |
| Servos | 3x MG996R or similar |
| Electromagnet | 5V DC electromagnet |
| Laser Module | 5mW red laser (calibration) |

## Pin Configuration

| Function | GPIO |
|----------|------|
| TFT_CS | 12 |
| TFT_DC | 1 (TXD0) |
| TFT_DIN | 15 |
| TFT_CLK | 2 |
| I2C_SDA | 13 |
| I2C_SCL | 14 |
| PCA_OE | 3 (RXD0) |

## Quick Start

1. **Install Dependencies**
   - Arduino IDE 2.x
   - ESP32 Board Package
   - TFT_eSPI library
   - Adafruit PWM Servo Driver library

2. **Configure TFT_eSPI**
   - Set display to ST7789V
   - Resolution: 240x320

3. **Upload Firmware**
   - Open `firmware/esp32_tictactoe_robot/esp32_tictactoe_robot.ino`
   - Select "AI Thinker ESP32-CAM" board
   - Upload

4. **Calibrate**
   - Press button during startup for WiFi mode
   - Connect to AP and configure WiFi
   - Use web interface to calibrate servos

See [docs/SETUP.md](docs/SETUP.md) for detailed instructions.

## Game Modes

### Normal Mode
Power on normally to play against the robot.

### Calibration Mode
Hold button during startup:
- WiFi configuration
- Servo position calibration
- Grid alignment calibration

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting PRs.

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file.

## Author

Ajeet Jain - [@ajeetjain94608](https://github.com/ajeetjain94608)

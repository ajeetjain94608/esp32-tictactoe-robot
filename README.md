# ESP32-CAM Tic-Tac-Toe Robot

A 5x5 Tic-Tac-Toe playing robot powered by ESP32-CAM with computer vision, servo-controlled arm, and TFT display.

## Features

- **5x5 Tic-Tac-Toe Game** - Extended board for more strategic gameplay
- **Computer Vision** - Camera-based piece detection (Red = Human, Blue = Robot)
- **Robotic Arm** - 3-servo arm with electromagnetic gripper for piece placement
- **TFT Display** - Graphical game UI with score tracking
- **Cheating Detection** - Detects and prevents invalid moves
- **WiFi Manager** - Easy WiFi configuration via AP mode
- **Web Calibration** - Browser-based servo and grid calibration

## Hardware

- ESP32-CAM (AI-Thinker)
- ST7789V TFT Display (240x320)
- PCA9685 Servo Controller
- 3x Servo Motors (Arm joints)
- Electromagnet (Piece gripper)
- Laser Module (Calibration aid)

## Modes

1. **Game Mode** - Play Tic-Tac-Toe against the robot
2. **WiFi/Calibration Mode** - Press button during startup for web-based calibration

## Game UI

- Score tracking (Human vs Robot wins)
- Graphical board display with colored pieces
- Move confirmation with progress bar
- Cheating warnings with board state reference
- Result screens with mini board display

## Setup

1. Flash the code to ESP32-CAM using Arduino IDE
2. Install required libraries:
   - TFT_eSPI
   - Adafruit PWM Servo Driver
   - ESP32 Camera
3. Configure TFT_eSPI for ST7789V (240x320)
4. Power on and calibrate servos via web interface

## License

MIT License

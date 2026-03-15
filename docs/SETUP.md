# Setup Guide

Complete setup instructions for the ESP32-CAM Tic-Tac-Toe Robot.

## Prerequisites

### Software
- Arduino IDE 2.x or later
- ESP32 Board Package (v2.0+)
- Required Libraries:
  - TFT_eSPI
  - Adafruit PWM Servo Driver
  - WiFi (built-in)
  - SPIFFS (built-in)

### Hardware
- ESP32-CAM (AI-Thinker)
- ST7789V TFT Display (240x320)
- PCA9685 Servo Driver
- 3x Servo Motors
- 5V Electromagnet
- 5V Power Supply (2A minimum)
- Game pieces (red and blue)

## Installation

### 1. Install Arduino IDE

Download from [arduino.cc](https://www.arduino.cc/en/software)

### 2. Install ESP32 Board Package

1. Open Arduino IDE
2. Go to File > Preferences
3. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to Tools > Board > Boards Manager
5. Search "ESP32" and install

### 3. Install Required Libraries

Go to Sketch > Include Library > Manage Libraries:

- **TFT_eSPI** by Bodmer
- **Adafruit PWM Servo Driver Library**

### 4. Configure TFT_eSPI

Edit the TFT_eSPI `User_Setup.h` file:

```cpp
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_CS   12
#define TFT_DC   1
#define TFT_RST  -1
#define TFT_MOSI 15
#define TFT_SCLK 2
```

### 5. Upload Firmware

1. Open `firmware/esp32_tictactoe_robot/esp32_tictactoe_robot.ino`
2. Select Board: "AI Thinker ESP32-CAM"
3. Select Port
4. Click Upload

## Hardware Assembly

### Wiring Diagram

```
ESP32-CAM          PCA9685
---------          -------
GPIO13 (SDA) ----> SDA
GPIO14 (SCL) ----> SCL
GND -------------> GND
5V --------------> VCC

PCA9685            Servos
-------            ------
Channel 0 -------> Alpha Servo (Base rotation)
Channel 1 -------> Beta Servo (Arm extension)
Channel 2 -------> Gamma Servo (Gripper height)
Channel 3 -------> Electromagnet

ESP32-CAM          TFT Display
---------          -----------
GPIO12 ----------> CS
GPIO1 (TX) ------> DC
GPIO15 ----------> MOSI/DIN
GPIO2 -----------> CLK
3.3V ------------> VCC
GND -------------> GND
```

### Servo Mounting

1. **Alpha Servo**: Base rotation (0-180 degrees)
2. **Beta Servo**: Arm extension
3. **Gamma Servo**: Vertical movement (gripper up/down)

## Calibration

### Enter Calibration Mode

1. Power off the robot
2. Press and hold the mode button
3. Power on while holding the button
4. Wait for "WiFi Mode" on display

### WiFi Configuration

1. Connect to AP: "TicTacToe-Robot"
2. Open browser: `http://192.168.4.1`
3. Enter your WiFi credentials
4. Save and restart

### Servo Calibration

1. Enter calibration mode
2. Connect to robot's WiFi
3. Open web interface
4. Use sliders to adjust:
   - Home position
   - Piece pickup position
   - Each board cell position
5. Save calibration

### Grid Calibration

1. Enable laser module
2. Adjust until laser points to board center
3. Fine-tune each cell position
4. Verify with test movements

## Troubleshooting

### Camera Not Working
- Check camera ribbon cable connection
- Verify camera module is OV2640
- Reset and try again

### Servos Jittering
- Check power supply (needs 2A+)
- Add capacitors to PCA9685
- Reduce servo speed in code

### TFT Display Blank
- Verify TFT_eSPI configuration
- Check wiring connections
- Ensure correct display driver (ST7789V)

### WiFi Not Connecting
- Verify credentials
- Check signal strength
- Reset and reconfigure

## Game Pieces

Use colored checkers or 3D printed pieces:
- **Red**: Human player (O)
- **Blue**: Robot player (X)

Pieces must be:
- Magnetic (for electromagnet gripper)
- Consistent size (~20mm diameter)
- Distinguishable colors for camera detection

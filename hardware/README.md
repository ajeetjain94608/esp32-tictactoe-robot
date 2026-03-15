# Hardware Documentation

This folder contains hardware-related files for the ESP32-CAM Tic-Tac-Toe Robot.

## Contents

- `schematics/` - Circuit diagrams and PCB designs

## Bill of Materials (BOM)

| Qty | Component | Description | Notes |
|-----|-----------|-------------|-------|
| 1 | ESP32-CAM | AI-Thinker module | With OV2640 camera |
| 1 | TFT Display | ST7789V 240x320 | SPI interface |
| 1 | PCA9685 | 16-ch PWM driver | I2C interface |
| 3 | Servo Motor | MG996R or similar | High torque |
| 1 | Electromagnet | 5V DC | For piece pickup |
| 1 | Laser Module | 5mW red | Calibration aid |
| 1 | Power Supply | 5V 3A | Adequate current |
| 1 | Buck Converter | 5V output | If using higher voltage |
| - | Wires | Jumper wires | Various lengths |
| 1 | PCB | Custom or perfboard | Optional |

## Mechanical Parts

- 3D printed arm segments
- 3D printed gripper
- Mounting brackets
- Game board (5x5 grid)
- Game pieces (magnetic, red and blue)

## Power Requirements

- ESP32-CAM: 5V, ~200mA
- TFT Display: 3.3V, ~50mA
- PCA9685: 5V, ~10mA (logic)
- Servos: 5V, ~500mA each (under load)
- Electromagnet: 5V, ~200mA

**Total**: 5V @ 2-3A recommended

## Assembly Tips

1. Use a common ground for all components
2. Add 100uF capacitor near servo power input
3. Keep camera ribbon cable away from servo wires
4. Secure all connections before testing

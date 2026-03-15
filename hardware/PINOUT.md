# ESP32-CAM Tic-Tac-Toe Robot - Pinout Reference

## ESP32-CAM (AI-Thinker) Pin Assignments

### TFT Display (ST7789V - 240x320)

| Signal | GPIO | Notes |
|--------|------|-------|
| TFT_CS | 12 | Chip Select (Active Low) |
| TFT_DC | 1 | Data/Command (shared with TXD0) |
| TFT_MOSI | 15 | SPI Data In |
| TFT_CLK | 2 | SPI Clock |
| TFT_RST | -1 | Not connected (use EN reset) |
| VCC | 3.3V | Power |
| GND | GND | Ground |

### I2C Bus (PCA9685 Servo Driver)

| Signal | GPIO | Notes |
|--------|------|-------|
| SDA | 13 | I2C Data |
| SCL | 14 | I2C Clock |
| PCA_OE | 3 | Output Enable (Active Low, shared with RXD0) |

### Control Pins

| Function | GPIO | Notes |
|----------|------|-------|
| Mode Button | 0 | WiFi/Game mode select (shared with XCLK) |
| Red LED | 33 | Built-in LED on ESP32-CAM |
| Flash LED | 4 | High-power white LED (solder pad) |

### Camera (OV2640)

| Signal | GPIO | Notes |
|--------|------|-------|
| PWDN | 32 | Power Down |
| XCLK | 0 | Master Clock (shared with button) |
| SIOD | 26 | SCCB Data (I2C-like) |
| SIOC | 27 | SCCB Clock |
| D7 (Y9) | 35 | Data bit 7 |
| D6 (Y8) | 34 | Data bit 6 |
| D5 (Y7) | 39 | Data bit 5 |
| D4 (Y6) | 36 | Data bit 4 |
| D3 (Y5) | 21 | Data bit 3 |
| D2 (Y4) | 19 | Data bit 2 |
| D1 (Y3) | 18 | Data bit 1 |
| D0 (Y2) | 5 | Data bit 0 |
| VSYNC | 25 | Vertical Sync |
| HREF | 23 | Horizontal Reference |
| PCLK | 22 | Pixel Clock |

---

## PCA9685 PWM Channel Assignments

| Channel | Function | Description |
|---------|----------|-------------|
| 0 | Alpha Servo | Base rotation (0-180°) |
| 1 | Beta Servo | Arm extension |
| 2 | Gamma Servo | Gripper height (up/down) |
| 3 | - | Reserved |
| 4 | - | Reserved |
| 5 | - | Reserved |
| 6 | Laser | Calibration laser module |
| 7 | Electromagnet | Piece gripper |

### PCA9685 Configuration
- I2C Address: 0x40 (default)
- PWM Frequency: 50 Hz (servo standard)
- Servo pulse range: 600-2400 µs

---

## Connection Diagram

```
                    ┌─────────────────────┐
                    │    ESP32-CAM        │
                    │   (AI-Thinker)      │
                    │                     │
         ┌──────────┤ GPIO0  ────────────►│ Mode Button + XCLK
         │          │ GPIO1  ────────────►│ TFT_DC (Soft TX)
         │          │ GPIO2  ────────────►│ TFT_CLK
         │          │ GPIO3  ────────────►│ PCA_OE (Active Low)
         │          │ GPIO4  ────────────►│ Flash LED
         │          │ GPIO12 ────────────►│ TFT_CS
         │          │ GPIO13 ────────────►│ I2C SDA
         │          │ GPIO14 ────────────►│ I2C SCL
         │          │ GPIO15 ────────────►│ TFT_MOSI
         │          │ GPIO33 ────────────►│ Red LED (built-in)
         │          │                     │
         │          │ 3.3V   ────────────►│ TFT VCC
         │          │ 5V     ────────────►│ PCA9685 VCC
         │          │ GND    ────────────►│ Common Ground
         │          └─────────────────────┘
         │
         │          ┌─────────────────────┐
         │          │     PCA9685         │
         │          │  (Servo Driver)     │
         └─────────►│ SDA ◄───── GPIO13   │
                    │ SCL ◄───── GPIO14   │
                    │ OE  ◄───── GPIO3    │
                    │                     │
                    │ CH0 ───────────────►│ Alpha Servo
                    │ CH1 ───────────────►│ Beta Servo
                    │ CH2 ───────────────►│ Gamma Servo
                    │ CH6 ───────────────►│ Laser Module
                    │ CH7 ───────────────►│ Electromagnet
                    │                     │
                    │ V+  ◄───── 5V       │ (Servo Power)
                    │ GND ◄───── GND      │
                    └─────────────────────┘
```

---

## Wiring Tables

### ESP32-CAM to TFT Display

| ESP32-CAM | TFT Display | Wire Color (suggested) |
|-----------|-------------|------------------------|
| GPIO12 | CS | Yellow |
| GPIO1 | DC | Orange |
| GPIO15 | MOSI/DIN | Blue |
| GPIO2 | CLK/SCK | Green |
| 3.3V | VCC | Red |
| GND | GND | Black |

### ESP32-CAM to PCA9685

| ESP32-CAM | PCA9685 | Wire Color (suggested) |
|-----------|---------|------------------------|
| GPIO13 | SDA | Blue |
| GPIO14 | SCL | Yellow |
| GPIO3 | OE | Orange |
| 5V | VCC | Red |
| GND | GND | Black |

### PCA9685 to Servos

| PCA9685 | Servo | Wire |
|---------|-------|------|
| CH0 PWM | Alpha Signal | Orange/Yellow |
| CH0 V+ | Alpha VCC | Red |
| CH0 GND | Alpha GND | Brown/Black |
| CH1 PWM | Beta Signal | Orange/Yellow |
| CH1 V+ | Beta VCC | Red |
| CH1 GND | Beta GND | Brown/Black |
| CH2 PWM | Gamma Signal | Orange/Yellow |
| CH2 V+ | Gamma VCC | Red |
| CH2 GND | Gamma GND | Brown/Black |

### PCA9685 to Peripherals

| PCA9685 | Device | Notes |
|---------|--------|-------|
| CH6 PWM | Laser + | Via transistor/MOSFET |
| CH6 GND | Laser - | Common ground |
| CH7 PWM | Electromagnet + | Via transistor/MOSFET |
| CH7 GND | Electromagnet - | Common ground |

---

## GPIO Summary Table

| GPIO | Function | Direction | Shared With |
|------|----------|-----------|-------------|
| 0 | Mode Button / XCLK | Input/Output | Camera clock |
| 1 | TFT_DC / Soft TX | Output | UART TX |
| 2 | TFT_CLK | Output | - |
| 3 | PCA_OE | Output | UART RX |
| 4 | Flash LED | Output | - |
| 5 | Camera D0 | Input | - |
| 12 | TFT_CS | Output | - |
| 13 | I2C SDA | Bidirectional | - |
| 14 | I2C SCL | Output | - |
| 15 | TFT_MOSI | Output | - |
| 18 | Camera D1 | Input | - |
| 19 | Camera D2 | Input | - |
| 21 | Camera D3 | Input | - |
| 22 | Camera PCLK | Input | - |
| 23 | Camera HREF | Input | - |
| 25 | Camera VSYNC | Input | - |
| 26 | Camera SIOD | Bidirectional | - |
| 27 | Camera SIOC | Output | - |
| 32 | Camera PWDN | Output | - |
| 33 | Red LED | Output | - |
| 34 | Camera D6 | Input | - |
| 35 | Camera D7 | Input | - |
| 36 | Camera D4 | Input | - |
| 39 | Camera D5 | Input | - |

---

## Power Requirements

| Component | Voltage | Current (max) |
|-----------|---------|---------------|
| ESP32-CAM | 5V | 310 mA |
| TFT Display | 3.3V | 50 mA |
| PCA9685 (logic) | 5V | 10 mA |
| Servo (each) | 5V | 500-700 mA |
| Electromagnet | 5V | 200-500 mA |
| Laser Module | 5V | 30 mA |
| **Total** | **5V** | **~2.5-3A** |

**Recommended Power Supply:** 5V 3A DC adapter

---

## Important Notes

1. **GPIO0 Multi-use:** Used for boot mode, mode button, and camera clock. Button only checked during startup before camera init.

2. **GPIO1/GPIO3:** These are UART TX/RX pins. TFT_DC uses GPIO1, PCA_OE uses GPIO3. Serial debugging not available during normal operation.

3. **TFT_CS Control:** Must set GPIO12 HIGH to disable TFT before using GPIO1 for other purposes (soft serial).

4. **PCA_OE Active Low:** Set GPIO3 LOW to enable PCA9685 outputs.

5. **Servo Power:** Connect servo power (V+) to external 5V supply through PCA9685 terminal block, not through ESP32-CAM 5V pin.

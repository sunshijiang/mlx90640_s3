# MLX90640 ESP32-S3 Thermal Camera Project

## Project Overview
ESP-IDF v5.3.1 project for MLX90640 thermal camera sensor on ESP32-S3. Reads 32x24 thermal matrix, applies calibration, and outputs temperature data via serial.

## Architecture
- **main.c**: Core application loop - I2C init, EEPROM read, sensor config, continuous thermal data acquisition
- **mlx90640_i2c_driver.c/h**: ESP-IDF I2C master API wrapper for MLX90640 communication
- **mlx90640_lib/**: MLX90640 API library (API.c/h, defs.h, reg.h) - handles calibration, temperature calculation
- **demo/**: LCD display examples (HX8347, ST7789 drivers)

## Key Patterns
- **I2C Dual-Speed Operation**: EEPROM read at 100kHz, data acquisition at 400kHz using `i2c_master_bus_rm_device()`/`i2c_master_bus_add_device()`
- **Chess Mode Acquisition**: Reads two consecutive frames to reconstruct complete 32x24 matrix (MLX90640_CalculateTo updates alternating pixels)
- **Bad Pixel Correction**: Applies `MLX90640_BadPixelsCorrection()` for broken/outlier pixels after each frame pair
- **Temperature Compensation**: Uses emissivity (0.95) and reflected temperature (Ta - 8°C) for accurate readings
- **Big-Endian I2C**: All MLX90640 registers use big-endian byte order

## Build & Debug Workflow
```bash
idf.py build          # Standard ESP-IDF build
idf.py flash          # Flash to ESP32-S3
idf.py monitor        # Serial monitor (115200 baud)
idf.py flash monitor  # Combined flash + monitor
```

## Integration Points
- **I2C Pins**: SDA=47, SCL=10 (configurable in main.c defines)
- **Power Control**: GPIO11 controls MLX_VDD (active low)
- **Sensor Address**: 0x33 (fixed)
- **External Dependencies**: Requires MLX90640 library files in mlx90640_lib/

## Conventions
- **Error Handling**: Uses ESP_ERROR_CHECK() for critical operations, ESP_LOGx() for diagnostics
- **Task Pinning**: MLX90640 task runs on core 1 with 8KB stack
- **Configuration**: All sensor/pin configs in main.c defines at top
- **Data Flow**: Raw frames → calibration → temperature matrix → statistics output
- **Debug Output**: Center 4x4 region printed by default; full matrix via PRINT_FULL_MATRIX define</content>
<parameter name="filePath">d:\esp\mlx90640_s3\.github\copilot-instructions.md
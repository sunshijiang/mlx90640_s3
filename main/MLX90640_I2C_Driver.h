#pragma once

#include <stdint.h>

int MLX90640_I2CInit(void);
int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t reg, uint16_t len, uint16_t *data);
int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t reg, uint16_t data);
int MLX90640_I2CGeneralReset(void);

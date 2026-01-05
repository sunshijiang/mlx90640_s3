#ifndef MLX90640_I2C_DRIVER_H
#define MLX90640_I2C_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// I2C 初始化函数
void MLX90640_I2CInit(uint8_t slaveAddr);

// I2C 读取函数 - 返回 0 表示成功，非 0 表示失败
uint8_t MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, 
                        uint16_t nMemAddressRead, uint16_t *data);

// I2C 写入函数 - 返回 0 表示成功，非 0 表示失败
uint8_t MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);

#ifdef __cplusplus
}
#endif

#endif // MLX90640_I2C_DRIVER_H

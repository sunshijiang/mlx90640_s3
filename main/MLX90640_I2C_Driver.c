/**
 * MLX90640 I2C driver for ESP-IDF
 * Compatible with official Melexis MLX90640 API
 */

#include "MLX90640_I2C_Driver.h"

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

/* ================= 用户可配置项 ================= */

#ifndef MLX_I2C_PORT
#define MLX_I2C_PORT I2C_NUM_0
#endif

#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 32    // 与 Arduino Wire 默认一致
#endif

#define I2C_TIMEOUT_MS 50

/* ================= 接口实现 ================= */

/**
 * 初始化 I2C
 * 说明：
 * ESP-IDF 中 I2C 通常在 main.c 里初始化，
 * 这里保留函数以兼容 MLX90640_API
 */
void MLX90640_I2CInit(void)
{
    /* 空实现即可 */
}

/**
 * 通用 I2C reset（可选）
 * MLX90640 官方 API 需要此符号
 */
int MLX90640_I2CGeneralReset(void)
{
    /* ESP32 不支持 I2C General Call Reset
       返回 0 以满足 API 逻辑 */
    return 0;
}

/**
 * 从 MLX90640 读取 nMemAddressRead 个 16-bit word
 * @return 0 成功，-1 失败
 */
int MLX90640_I2CRead(uint8_t slaveAddr,
                     uint16_t startAddress,
                     uint16_t nMemAddressRead,
                     uint16_t *data)
{
    uint16_t bytesRemaining = nMemAddressRead * 2;
    uint16_t dataIndex = 0;

    while (bytesRemaining > 0)
    {
        uint16_t bytesToRead = bytesRemaining;
        if (bytesToRead > I2C_BUFFER_LENGTH)
            bytesToRead = I2C_BUFFER_LENGTH;

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        /* ---------- 写寄存器地址（不发送 STOP） ---------- */
        i2c_master_start(cmd);
        i2c_master_write_byte(
            cmd,
            (slaveAddr << 1) | I2C_MASTER_WRITE,
            true);

        i2c_master_write_byte(cmd, startAddress >> 8, true);
        i2c_master_write_byte(cmd, startAddress & 0xFF, true);

        /* ---------- Repeated START 读数据 ---------- */
        i2c_master_start(cmd);
        i2c_master_write_byte(
            cmd,
            (slaveAddr << 1) | I2C_MASTER_READ,
            true);

        for (uint16_t i = 0; i < bytesToRead; i++)
        {
            uint8_t byte;
            i2c_master_read_byte(
                cmd,
                &byte,
                (i == bytesToRead - 1) ?
                    I2C_MASTER_NACK :
                    I2C_MASTER_ACK);

            if ((i & 1) == 0)
                data[dataIndex] = ((uint16_t)byte) << 8;
            else
                data[dataIndex++] |= byte;
        }

        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(
            MLX_I2C_PORT,
            cmd,
            pdMS_TO_TICKS(I2C_TIMEOUT_MS));

        i2c_cmd_link_delete(cmd);

        if (ret != ESP_OK)
            return -1;

        bytesRemaining -= bytesToRead;
        startAddress   += bytesToRead / 2;
    }

    return 0;
}

/**
 * 写一个 16-bit word
 * @return 0 成功，-1 ACK 错误，-2 校验失败
 */
int MLX90640_I2CWrite(uint8_t slaveAddr,
                      uint16_t writeAddress,
                      uint16_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(
        cmd,
        (slaveAddr << 1) | I2C_MASTER_WRITE,
        true);

    i2c_master_write_byte(cmd, writeAddress >> 8, true);
    i2c_master_write_byte(cmd, writeAddress & 0xFF, true);
    i2c_master_write_byte(cmd, data >> 8, true);
    i2c_master_write_byte(cmd, data & 0xFF, true);

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(
        MLX_I2C_PORT,
        cmd,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS));

    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
        return -1;

    /* ---------- 写后校验（官方要求） ---------- */
    uint16_t dataCheck;
    if (MLX90640_I2CRead(slaveAddr, writeAddress, 1, &dataCheck) != 0)
        return -1;

    if (dataCheck != data)
        return -2;

    return 0;
}

/**
 * 设置 I2C 频率（kHz）
 * ESP-IDF 不支持运行时修改，此函数仅作兼容
 */
void MLX90640_I2CFreqSet(int freq)
{
    (void)freq;
}

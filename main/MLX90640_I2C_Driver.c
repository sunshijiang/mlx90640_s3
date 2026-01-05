#include "mlx90640_i2c_driver.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"

#define TAG "MLX90640_I2C"

// 外部声明的 I2C 设备句柄
extern i2c_master_dev_handle_t dev_handle;

// I2C 初始化函数
void MLX90640_I2CInit(uint8_t slaveAddr)
{
    // 在 main.c 中初始化，这里只需声明
    ESP_LOGI(TAG, "MLX90640_I2CInit called for slave address 0x%02X", slaveAddr);
}

// I2C 读取函数
uint8_t MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, 
                        uint16_t nMemAddressRead, uint16_t *data)
{
    esp_err_t ret;
    uint8_t cmd[2];
    uint8_t rx_buf[2];
    
    // 检查设备句柄
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "I2C device not initialized");
        return 1;
    }
    
    // 发送寄存器地址（大端序）
    cmd[0] = (uint8_t)(startAddress >> 8);   // 高字节
    cmd[1] = (uint8_t)(startAddress & 0xFF); // 低字节
    
    ret = i2c_master_transmit(dev_handle, cmd, 2, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write address failed: %s", esp_err_to_name(ret));
        return 1;
    }
    
    // 读取数据
    for (int i = 0; i < nMemAddressRead; i++) {
        ret = i2c_master_receive(dev_handle, rx_buf, 2, 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C read data failed: %s", esp_err_to_name(ret));
            return 1;
        }
        
        // MLX90640 使用大端序，需要转换为小端序
        data[i] = (rx_buf[0] << 8) | rx_buf[1];
    }
    
    return 0; // 成功
}

// I2C 写入函数
uint8_t MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    esp_err_t ret;
    uint8_t tx_buf[4];
    
    // 检查设备句柄
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "I2C device not initialized");
        return 1;
    }
    
    // 构造发送数据（大端序）
    tx_buf[0] = (uint8_t)(writeAddress >> 8);   // 地址高字节
    tx_buf[1] = (uint8_t)(writeAddress & 0xFF); // 地址低字节
    tx_buf[2] = (uint8_t)(data >> 8);           // 数据高字节
    tx_buf[3] = (uint8_t)(data & 0xFF);         // 数据低字节
    
    ret = i2c_master_transmit(dev_handle, tx_buf, 4, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
        return 1;
    }
    
    return 0; // 成功
}

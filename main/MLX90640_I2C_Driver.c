#include "MLX90640_I2C_Driver.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "MLX90640_I2C"

/* ===== I2C 硬件配置 ===== */
#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_SDA_GPIO    47
#define I2C_SCL_GPIO    10
#define I2C_FREQ_HZ     100000   // EEPROM 阶段稳定优先

static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

/* ================= 初始化 ================= */
esp_err_t MLX90640_I2CInit(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = 0x33,
        .scl_speed_hz    = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle)
    );

    ESP_LOGI(TAG, "I2C master initialized");
    return ESP_OK;
}

/* ================= MLX90640 API 兼容接口 ================= */

void MLX90640_I2CFreqSet(int freq)
{
    (void)freq; // ESP-IDF 5.x 不支持运行时改速
}

int MLX90640_I2CGeneralReset(void)
{
    return 0;
}

int MLX90640_I2CRead(uint8_t slaveAddr,
                     uint16_t startAddress,
                     uint16_t nWords,
                     uint16_t *data)
{
    uint8_t reg[2] = {
        startAddress >> 8,
        startAddress & 0xFF
    };

    esp_err_t ret = i2c_master_transmit_receive(
        dev_handle,
        reg,
        2,
        (uint8_t *)data,
        nWords * 2,
        pdMS_TO_TICKS(200)
    );

    if (ret != ESP_OK)
        return -1;

    /* MLX90640 是 big-endian */
    for (int i = 0; i < nWords; i++) {
        data[i] = (data[i] << 8) | (data[i] >> 8);
    }

    return 0;
}

int MLX90640_I2CWrite(uint8_t slaveAddr,
                      uint16_t writeAddress,
                      uint16_t data)
{
    uint8_t buf[4] = {
        writeAddress >> 8,
        writeAddress & 0xFF,
        data >> 8,
        data & 0xFF
    };

    esp_err_t ret = i2c_master_transmit(
        dev_handle,
        buf,
        4,
        pdMS_TO_TICKS(200)
    );

    return (ret == ESP_OK) ? 0 : -1;
}

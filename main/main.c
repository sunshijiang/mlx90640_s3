/**
 * @file main.c
 * @brief MLX90640 ESP-IDF 主程序（使用新 I2C Master API）
 * @details 基于 ESP-IDF v5.3.1，修复了格式化字符串类型不匹配问题
 * @date 2026-01-05
 */

#include <stdio.h>
#include <inttypes.h>  // 必须包含，用于 PRIu32 等格式化宏
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

/* ================= 配置参数 ================= */
#define TAG "MLX90640"

#define MLX90640_ADDR      0x33        // MLX90640 I2C 地址
#define MLX_VDD_PIN        GPIO_NUM_11 // MLX90640 电源控制引脚
#define I2C_SDA_PIN        GPIO_NUM_47 // I2C SDA 引脚
#define I2C_SCL_PIN        GPIO_NUM_10 // I2C SCL 引脚
#define I2C_PORT           I2C_NUM_0    // I2C 端口号

#define I2C_FREQ_EE_HZ     100000      // EEPROM 读取频率（100kHz）
#define I2C_FREQ_RUN_HZ    400000      // 运行频率（400kHz）

#define TA_SHIFT           8           // 环境温度偏移量
#define EMISSIVITY         0.95f       // 发射率（人体皮肤约 0.95）

/* ================= 全局变量 ================= */
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;
static paramsMLX90640 mlx90640;
static float mlx90640To[768];          // 32x24 = 768 个温度点

/* ================= I2C 初始化函数 ================= */
static esp_err_t mlx90640_i2c_init(uint32_t freq_hz)
{
    esp_err_t ret;
    
    // 如果已存在，先清理
    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
    }
    if (bus_handle) {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }
    
    // 1. 配置并创建 I2C 总线
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, // 使用外部上拉电阻
    };
    
    ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 2. 添加 MLX90640 设备到总线
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MLX90640_ADDR,
        .scl_speed_hz = freq_hz,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
        return ret;
    }
    
    // 使用正确的格式化字符串：%lu 或 PRIu32
    ESP_LOGI(TAG, "I2C initialized at %" PRIu32 " Hz", freq_hz);
    return ESP_OK;
}

/* ================= MLX90640 任务函数 ================= */
static void mlx90640_task(void *arg)
{
    ESP_LOGI(TAG, "MLX90640 task started");
    
    // 1. 初始化 I2C（低速模式用于 EEPROM 读取）
    ESP_ERROR_CHECK(mlx90640_i2c_init(I2C_FREQ_EE_HZ));
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待传感器稳定
    
    // 2. 读取 EEPROM 参数
    uint16_t eeMLX90640[832];
    int status = MLX90640_DumpEE(MLX90640_ADDR, eeMLX90640);
    if (status != 0) {
        ESP_LOGE(TAG, "MLX90640 EEPROM read failed: %d", status);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "MLX90640 detected (EEPROM OK)");
    
    // 3. 提取参数
    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0) {
        ESP_LOGE(TAG, "ExtractParameters failed: %d", status);
        // 打印 EEPROM 前几个值用于调试
        ESP_LOG_BUFFER_HEXDUMP("EEPROM", eeMLX90640, 32, ESP_LOG_ERROR);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Parameters extracted successfully");
    
    // 4. 切换到高速模式
    i2c_master_bus_rm_device(dev_handle);
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MLX90640_ADDR,
        .scl_speed_hz = I2C_FREQ_RUN_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
    ESP_LOGI(TAG, "I2C switched to %" PRIu32 " Hz", I2C_FREQ_RUN_HZ);
    
    // 5. 配置传感器
    MLX90640_SetRefreshRate(MLX90640_ADDR, 0x05); // 8Hz
    MLX90640_SetChessMode(MLX90640_ADDR);         // 棋盘模式（噪声更低）
    ESP_LOGI(TAG, "MLX90640 configured: 8Hz, Chess mode");
    
    // 6. 主循环
    while (1) {
        uint16_t frame[834];
        
        // 读取一帧数据
        status = MLX90640_GetFrameData(MLX90640_ADDR, frame);
        if (status < 0) {
            ESP_LOGW(TAG, "GetFrameData error: %d", status);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // 计算环境温度
        float vdd = MLX90640_GetVdd(frame, &mlx90640);
        float Ta = MLX90640_GetTa(frame, &mlx90640);
        
        // 计算目标温度
        float tr = Ta - TA_SHIFT; // 反射温度补偿
        MLX90640_CalculateTo(frame, &mlx90640, EMISSIVITY, tr, mlx90640To);
        
        // 坏点校正
        MLX90640_BadPixelsCorrection(mlx90640.brokenPixels, mlx90640To, 1, &mlx90640);
        MLX90640_BadPixelsCorrection(mlx90640.outlierPixels, mlx90640To, 1, &mlx90640);
        
        // 统计温度
        float minTemp = mlx90640To[0];
        float maxTemp = mlx90640To[0];
        float sumTemp = 0.0f;
        
        for (int i = 0; i < 768; i++) {
            if (mlx90640To[i] < minTemp) minTemp = mlx90640To[i];
            if (mlx90640To[i] > maxTemp) maxTemp = mlx90640To[i];
            sumTemp += mlx90640To[i];
        }
        float avgTemp = sumTemp / 768.0f;
        
        // 打印温度信息（使用正确的格式化）
        ESP_LOGI(TAG, "Ta=%.2f°C, Vdd=%.2fV, Min=%.2f°C, Max=%.2f°C, Avg=%.2f°C",
                 Ta, vdd, minTemp, maxTemp, avgTemp);
        
        // 打印中心区域温度（16,12）
        float centerTemp = mlx90640To[12 * 32 + 16];
        ESP_LOGI(TAG, "Center Temp (16,12): %.2f°C", centerTemp);
        
        vTaskDelay(pdMS_TO_TICKS(125)); // 8Hz 对应 125ms
    }
}

/* ================= 应用程序入口 ================= */
void app_main(void)
{
    ESP_LOGI(TAG, "MLX90640 ESP-IDF Example");
    
    // 1. 配置 MLX90640 电源控制引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MLX_VDD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(MLX_VDD_PIN, 0); // 使能 MLX90640 电源
    vTaskDelay(pdMS_TO_TICKS(50));  // 等待电源稳定
    
    // 2. 创建 MLX90640 任务
    xTaskCreatePinnedToCore(
        mlx90640_task,
        "mlx90640_task",
        8192,        // 堆栈大小
        NULL,
        5,           // 优先级
        NULL,
        1            // 运行在核心 1
    );
    
    ESP_LOGI(TAG, "MLX90640 task created on core 1");
}

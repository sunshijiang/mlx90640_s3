#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_err.h"

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

/* ================= 配置 ================= */
#define TAG "MLX90640"

#define MLX90640_ADDR  0x33

#define MLX_VDD_PIN    GPIO_NUM_11
#define I2C_SDA_PIN    GPIO_NUM_47
#define I2C_SCL_PIN    GPIO_NUM_10

#define I2C_PORT       I2C_NUM_0
#define I2C_FREQ_HZ    800000

#define TA_SHIFT       8

/* ================= 全局 ================= */
static paramsMLX90640 mlx90640;
static float mlx90640To[768];

/* ================= I2C 初始化 ================= */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    return i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

/* ================= MLX90640 任务 ================= */
static void mlx90640_task(void *arg)
{
    ESP_LOGI(TAG, "MLX90640 task start");

    ESP_ERROR_CHECK(i2c_master_init());

    ESP_LOGI(TAG, "I2C initialized");

    /* ===== 直接从 EEPROM 读开始（即 Probe） ===== */

    uint16_t eeMLX90640[832];
    int status = MLX90640_DumpEE(MLX90640_ADDR, eeMLX90640);
    if (status != 0) {
        ESP_LOGE(TAG, "MLX90640 EEPROM read failed: %d", status);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "MLX90640 detected (EEPROM OK)");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0) {
        ESP_LOGE(TAG, "ExtractParameters failed: %d", status);
        vTaskDelete(NULL);
    }

    MLX90640_SetRefreshRate(MLX90640_ADDR, 0x05); // 8Hz

    ESP_LOGI(TAG, "MLX90640 init OK");

    while (1) {
        uint16_t frame[834];

        int ret = MLX90640_GetFrameData(MLX90640_ADDR, frame);
        if (ret < 0) {
            ESP_LOGW(TAG, "GetFrame error: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        float vdd = MLX90640_GetVdd(frame, &mlx90640);
        float Ta  = MLX90640_GetTa(frame, &mlx90640);

        float tr = Ta - TA_SHIFT;
        float emissivity = 0.95f;

        MLX90640_CalculateTo(frame, &mlx90640, emissivity, tr, mlx90640To);

        float center = mlx90640To[12 * 32 + 16];
        ESP_LOGI(TAG,
                 "Ta=%.2fC Center=%.2fC Vdd=%.2fV",
                 Ta, center, vdd);

        vTaskDelay(pdMS_TO_TICKS(125));
    }
}


/* ================= app_main ================= */
void app_main(void)
{
    ESP_LOGI(TAG, "MLX90640 ESP-IDF example");

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << MLX_VDD_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(MLX_VDD_PIN, 0);

    vTaskDelay(pdMS_TO_TICKS(500));

    xTaskCreatePinnedToCore(
        mlx90640_task,
        "mlx90640",
        8192,
        NULL,
        5,
        NULL,
        1
    );
}

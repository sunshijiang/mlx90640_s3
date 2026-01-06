#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

/* ================= 用户配置 ================= */
#define TAG "MLX90640"

/* I2C & GPIO */
#define I2C_SDA_GPIO    GPIO_NUM_47
#define I2C_SCL_GPIO    GPIO_NUM_10
#define I2C_PULL_GPIO   GPIO_NUM_11   // 控制外部上拉（低电平使能）

#define MLX90640_ADDR   0x33
#define TA_SHIFT        8

/* ================= 全局 ================= */
static paramsMLX90640 mlx90640;
static float mlx90640To[768];

/* ================= 任务 ================= */
static void mlx90640_task(void *arg)
{
    ESP_LOGI(TAG, "MLX90640 task start");

    /* 初始化 I2C（在 I2C driver 内部完成） */
    ESP_ERROR_CHECK(MLX90640_I2CInit());

    uint16_t eeData[832];

    int ret = MLX90640_DumpEE(MLX90640_ADDR, eeData);
    if (ret != 0) {
        ESP_LOGE(TAG, "EEPROM read failed: %d", ret);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "EEPROM OK");

    ret = MLX90640_ExtractParameters(eeData, &mlx90640);
    if (ret != 0) {
        ESP_LOGE(TAG, "ExtractParameters failed: %d", ret);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Parameters extracted");

    MLX90640_SetRefreshRate(MLX90640_ADDR, 0x04); // 4Hz

    while (1) {
        uint16_t frame[834];

        ret = MLX90640_GetFrameData(MLX90640_ADDR, frame);
        if (ret < 0) {
            ESP_LOGW(TAG, "Frame error: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        float Ta  = MLX90640_GetTa(frame, &mlx90640);
        float vdd = MLX90640_GetVdd(frame, &mlx90640);

        float tr = Ta - TA_SHIFT;
        MLX90640_CalculateTo(frame, &mlx90640, 0.95f, tr, mlx90640To);

        float center = mlx90640To[12 * 32 + 16];

        ESP_LOGI(TAG,
                 "Ta=%.2fC  Center=%.2fC  Vdd=%.2fV",
                 Ta, center, vdd);

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/* ================= app_main ================= */
void app_main(void)
{
    ESP_LOGI(TAG, "MLX90640 ESP-IDF example start");

    /* 打开 I2C 外部上拉 */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << I2C_PULL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(I2C_PULL_GPIO, 0); // 低电平=使能上拉

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

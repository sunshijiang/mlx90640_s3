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

#define BOOT_BUTTON_GPIO GPIO_NUM_0  // BOOT 按键
#define MLX90640_ADDR   0x33
#define TA_SHIFT        8

/* ================= 全局 ================= */
static paramsMLX90640 mlx90640;
static float mlx90640To[768];

/* ================= 按键初始化 ================= */
static void button_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
}

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
        // 检测按键按下
        if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
            ESP_LOGI(TAG, "Button pressed, reading full MLX90640 frame...");

            uint16_t frame[834];
            ret = MLX90640_GetFrameData(MLX90640_ADDR, frame);
            if (ret < 0) {
                ESP_LOGW(TAG, "Frame error: %d", ret);
            } else {
                float Ta  = MLX90640_GetTa(frame, &mlx90640);
                float vdd = MLX90640_GetVdd(frame, &mlx90640);

                float tr = Ta - TA_SHIFT;
                MLX90640_CalculateTo(frame, &mlx90640, 0.95f, tr, mlx90640To);

                ESP_LOGI(TAG, "Ta=%.2fC  Vdd=%.2fV  Full frame:", Ta, vdd);

                // 输出768个像素
                // for (int i = 0; i < 768; i++) {
                //     ESP_LOGI(TAG, "Pixel[%d]: %.2f C", i, mlx90640To[i]);
                // }

                ESP_LOGI(TAG, "Full frame (24x32):");

                for (int row = 0; row < 24; row++) {
                    char line[512];
                    int len = 0;

                    len += snprintf(line + len, sizeof(line) - len,
                                    "Row %02d: ", row);

                    for (int col = 0; col < 32; col++) {
                        int idx = row * 32 + col;
                        len += snprintf(line + len, sizeof(line) - len,
                                        "%6.2f ", mlx90640To[idx]);
                    }

                    ESP_LOGI(TAG, "%s", line);
                }


            }

            // 等待按键松开
            while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            ESP_LOGI(TAG, "Ready for next press.");
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms轮询按键
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

    button_init();

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

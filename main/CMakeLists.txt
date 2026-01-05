idf_component_register(SRCS "main.c" "mlx90640_lib/MLX90640_API.c" "mlx90640_i2c_driver.c"
                    INCLUDE_DIRS "." "mlx90640_lib"
    REQUIRES
        driver
        esp_timer
        freertos
        )
# idf_component_register(SRCS "main.c"
#                     INCLUDE_DIRS ".")

# idf_component_register(SRCS "main.cpp" "CST816T.cpp" "MLX90640_API.cpp" "MLX90640_I2C_Driver.cpp"
#                     INCLUDE_DIRS ".")

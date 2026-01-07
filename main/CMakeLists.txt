idf_component_register(SRCS "mlx90640_i2c_driver.c" "MLX90640_API.c" "main.c" "MLX90640_API.c" "MLX90640_I2C_Driver.c"
                    INCLUDE_DIRS "." 
    REQUIRES
        driver
        esp_driver_i2c
        esp_timer
        freertos
        )
# idf_component_register(SRCS "main.c"
#                     INCLUDE_DIRS ".")

# idf_component_register(SRCS "main.cpp" "CST816T.cpp" "MLX90640_API.cpp" "MLX90640_I2C_Driver.cpp"
#                     INCLUDE_DIRS ".")

idf_component_register(
    SRCS "sensors_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos log esp_driver_i2c esp_driver_gpio esp_timer config shared_state
)

idf_component_register(
    SRCS "gps_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos log esp_driver_uart config shared_state minmea
)

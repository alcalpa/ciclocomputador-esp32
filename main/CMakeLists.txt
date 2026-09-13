idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES log shared_state gps_task sensors_task ble_task storage_task display_task
)

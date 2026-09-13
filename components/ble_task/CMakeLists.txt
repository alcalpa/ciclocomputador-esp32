idf_component_register(
    SRCS "ble_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos log bt shared_state
)

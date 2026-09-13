idf_component_register(
    SRCS "sensors_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos log driver config shared_state
)

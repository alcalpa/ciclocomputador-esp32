idf_component_register(
    SRCS "storage_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos log driver fatfs sdmmc config shared_state
)

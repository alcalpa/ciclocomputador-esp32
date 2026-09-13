idf_component_register(
    SRCS
        "display_task.c"
        "renderer/renderer_breadcrumb.c"
    INCLUDE_DIRS
        "include"
        "renderer/include"
    REQUIRES freertos log driver config shared_state
)

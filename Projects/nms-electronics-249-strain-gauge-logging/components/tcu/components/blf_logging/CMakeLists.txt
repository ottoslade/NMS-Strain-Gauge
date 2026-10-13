idf_component_register(
    SRCS 
        "blf.c"
    INCLUDE_DIRS 
        ../../../../shared/blf
    REQUIRES 
        log
        fatfs
        sdmmc
        driver
        esp_timer
        freertos
        vfs
)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")

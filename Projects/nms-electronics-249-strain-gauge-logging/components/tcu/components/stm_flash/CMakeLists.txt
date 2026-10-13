idf_component_register(
    SRCS   
        "stm_flash.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        esp_event 
        freertos 
        spiffs 
        driver 
        fatfs 
        sdmmc 
        log 
        can
)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")

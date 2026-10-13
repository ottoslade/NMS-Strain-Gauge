idf_component_register(
    SRCS 
        "main.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver 
        stm_flash 
        update_server 
        can 
        esp_wifi 
        nvs_flash 
        telemetry
)

if(DEFINED CPU_DEBUG)
    add_compile_definitions(CPU_DEBUG)
endif()

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")
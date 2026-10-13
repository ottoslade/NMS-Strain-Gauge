idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES 
                    REQUIRES driver esp_wifi nvs_flash telemetry)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")
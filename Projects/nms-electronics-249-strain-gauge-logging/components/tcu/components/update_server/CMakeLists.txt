idf_component_register(
    SRCS 
        "update_server.c" 
        "../../../../shared/util/util.c"
    INCLUDE_DIRS 
        "include" 
        "../../../../shared/util"
    EMBED_FILES 
        "favicon.ico" 
        "upload_script.html"
    REQUIRES 
        stm_flash 
        esp_http_server 
        vfs 
        app_update 
        log
)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")
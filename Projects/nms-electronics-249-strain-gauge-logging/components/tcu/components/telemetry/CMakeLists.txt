idf_component_register(
    SRCS 
        "telemetry.c"
        "../../../../shared/util/util.c"
        "../../../../shared/nms_espnow_telemetry/nms_espnow_telemetry.c"
    INCLUDE_DIRS 
        "include"
        "../../../../shared/util"
        "../../../../shared/nms_espnow_telemetry"
    REQUIRES 
        esp_wifi 
        can 
        freertos 
        log
)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")

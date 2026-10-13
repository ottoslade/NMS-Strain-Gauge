idf_component_register(
    SRCS 
        "telemetry.c"
        "../../../../shared/nms_espnow_telemetry/nms_espnow_telemetry.c"
        "../../../../shared/util/util.c"
        "../../../../shared/nms_can/nms_can.c"
    INCLUDE_DIRS 
        "include"
        "../../../../shared/nms_espnow_telemetry"
        "../../../../shared/util"
        "../../../../shared/nms_can"
    REQUIRES 
        esp_wifi 
        util 
        esp_http_server 
        driver 
        freertos 
        log 
)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")
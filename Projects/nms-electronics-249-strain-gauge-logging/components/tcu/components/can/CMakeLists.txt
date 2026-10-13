idf_component_register(
    SRCS 
        "can.c" 
        "../../../../shared/dbc_to_c/generated_code/VEHICLE_CAN.c" 
        "../../../../shared/util/util.c"
        "../../../../shared/nms_can/nms_can.c"
    INCLUDE_DIRS 
        "include" 
        "../../../../shared/dbc_to_c/generated_code/"
        "../../../../shared/util/"
        "../../../../shared/nms_can"
    REQUIRES 
        freertos 
        driver 
        telemetry
)

idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PRIVATE "${FREERTOS_ORIG_INCLUDE_PATH}")
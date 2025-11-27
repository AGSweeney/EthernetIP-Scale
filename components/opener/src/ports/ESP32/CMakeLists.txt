idf_component_register(
    SRCS 
        "opener.c"
        "networkhandler.c"
        "networkconfig.c"
        "opener_error.c"
    INCLUDE_DIRS 
        "."
        "../.."
        "../../../cip"
        "../../../enet_encap"
        "../../../utils"
        "../../../ports"
        "../../../ports/nvdata"
    PRIV_INCLUDE_DIRS
        "scale_application"
    REQUIRES 
        lwip
        freertos
    PRIV_REQUIRES
        lwip
        freertos
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP32)


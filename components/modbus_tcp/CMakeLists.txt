idf_component_register(
    SRCS
        "src/modbus_tcp.c"
        "src/modbus_protocol.c"
        "src/modbus_register_map.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        lwip
        freertos
        esp_netif
)


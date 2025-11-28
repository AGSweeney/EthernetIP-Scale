cmake_minimum_required(VERSION 3.16)

set(OPENER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(OPENER_PORTS_DIR "${OPENER_SRC_DIR}/ports")
set(OPENER_ESP32_DIR "${OPENER_PORTS_DIR}/ESP32")

set(ESP32_PORT_SRCS
    "${OPENER_ESP32_DIR}/opener.c"
    "${OPENER_ESP32_DIR}/networkhandler.c"
    "${OPENER_ESP32_DIR}/networkconfig.c"
    "${OPENER_ESP32_DIR}/opener_error.c"
    "${OPENER_ESP32_DIR}/eth_media_counters.c"
    "${OPENER_ESP32_DIR}/scale_application/scaleapplication.c"
)

set(PORTS_GENERIC_SRCS
    "${OPENER_PORTS_DIR}/generic_networkhandler.c"
    "${OPENER_PORTS_DIR}/socket_timer.c"
)

set(CIP_SRCS
    "${OPENER_SRC_DIR}/cip/appcontype.c"
    "${OPENER_SRC_DIR}/cip/cipassembly.c"
    "${OPENER_SRC_DIR}/cip/cipclass3connection.c"
    "${OPENER_SRC_DIR}/cip/cipcommon.c"
    "${OPENER_SRC_DIR}/cip/cipconnectionmanager.c"
    "${OPENER_SRC_DIR}/cip/cipconnectionobject.c"
    "${OPENER_SRC_DIR}/cip/cipdlr.c"
    "${OPENER_SRC_DIR}/cip/cipelectronickey.c"
    "${OPENER_SRC_DIR}/cip/cipepath.c"
    "${OPENER_SRC_DIR}/cip/cipethernetlink.c"
    "${OPENER_SRC_DIR}/cip/cipidentity.c"
    "${OPENER_SRC_DIR}/cip/cipioconnection.c"
    "${OPENER_SRC_DIR}/cip/cipmessagerouter.c"
    "${OPENER_SRC_DIR}/cip/cipqos.c"
    "${OPENER_SRC_DIR}/cip/cipstring.c"
    "${OPENER_SRC_DIR}/cip/cipstringi.c"
    "${OPENER_SRC_DIR}/cip/ciptcpipinterface.c"
    "${OPENER_SRC_DIR}/cip/ciptypes.c"
)

set(ENET_ENCAP_SRCS
    "${OPENER_SRC_DIR}/enet_encap/cpf.c"
    "${OPENER_SRC_DIR}/enet_encap/encap.c"
    "${OPENER_SRC_DIR}/enet_encap/endianconv.c"
)

set(UTILS_SRCS
    "${OPENER_SRC_DIR}/utils/doublylinkedlist.c"
    "${OPENER_SRC_DIR}/utils/enipmessage.c"
    "${OPENER_SRC_DIR}/utils/random.c"
    "${OPENER_SRC_DIR}/utils/xorshiftrandom.c"
)

set(NVDATA_SRCS
    "${OPENER_PORTS_DIR}/nvdata/conffile.c"
    "${OPENER_PORTS_DIR}/nvdata/nvdata.c"
    "${OPENER_PORTS_DIR}/nvdata/nvqos.c"
    "${OPENER_PORTS_DIR}/nvdata/nvtcpip.c"
)

idf_component_register(
    SRCS 
        ${ESP32_PORT_SRCS}
        ${PORTS_GENERIC_SRCS}
        ${CIP_SRCS}
        ${ENET_ENCAP_SRCS}
        ${UTILS_SRCS}
        ${NVDATA_SRCS}
    INCLUDE_DIRS 
        "${OPENER_SRC_DIR}"
        "${OPENER_PORTS_DIR}"
        "${OPENER_ESP32_DIR}"
        "${OPENER_ESP32_DIR}/scale_application"
        "${OPENER_SRC_DIR}/cip"
        "${OPENER_SRC_DIR}/enet_encap"
        "${OPENER_SRC_DIR}/utils"
        "${OPENER_PORTS_DIR}/nvdata"
    REQUIRES 
        lwip
        freertos
        esp_eth
        esp_netif
        driver
        nvs_flash
        system_config
    PRIV_REQUIRES
        lwip
        freertos
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP32)


#===============================================================================
# @brief    cmake file for qrswork client
#===============================================================================

set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_client_main.c
)

set(PUBLIC_HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

if(DEFINED CONFIG_QRSWORK_NORTHBOUND_ENABLE)
    list(APPEND SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/northbound_client.c
        ${CMAKE_CURRENT_SOURCE_DIR}/wifi/wifi_connect.c
    )
    list(APPEND PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/wifi
    )
endif()

set(SOURCES "${SOURCES_LIST}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)

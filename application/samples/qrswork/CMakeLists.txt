if(DEFINED CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER)
    set(SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/ky.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/rgb.c
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server/qrswork_server_adv.c
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server/qrswork_server.c
    )

    set(PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server
    )
else()
    set(SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/ky.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/rgb.c
    )

    set(PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/
    )
endif()

set(SOURCES "${SOURCES_LIST}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)


add_subdirectory_if_exist(lib)
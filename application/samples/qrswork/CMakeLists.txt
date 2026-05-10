#===============================================================================
# @brief    cmake file for qrswork
#===============================================================================

# ======================================================================
# 头文件搜索路径：统一指向根目录 lib/（唯一真实代码源）
# ======================================================================
set(LIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib)

# lib/ 根目录：支持 #include "sensor/mpu6050.h"、#include "output/ssd1306.h" 等相对路径
include_directories(${LIB_ROOT})
# sensor/：支持 #include "mpu6050.h"、#include "ky039.h"
include_directories(${LIB_ROOT}/sensor)
# output/：支持 #include "ssd1306.h"、#include "rgb_led.h"
include_directories(${LIB_ROOT}/output)
# system/：支持 #include "i2c_master.h"
include_directories(${LIB_ROOT}/system)
# algorithm/：支持 #include "fall_detection.h"
include_directories(${LIB_ROOT}/algorithm)
# app/：支持 #include "health_monitor_main.h"
include_directories(${LIB_ROOT}/app)
# comm/：支持 #include "comm/sle_comm.h"
include_directories(${LIB_ROOT}/comm)
# test/：支持 #include "test/mpu6050_test.h"
include_directories(${LIB_ROOT}/test)
# qrswork_server/：支持 #include "qrswork_server.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server)
# cJSON：支持 #include "cJSON.h"（SDK 自带 open_source/cjson）
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../references/bearpi_3863_sdk/bearpi-pico_h3863/open_source/cjson/cjson)

# ======================================================================
# 源文件列表：全部指向根目录 lib/
# ======================================================================
if(DEFINED CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER)
    set(SOURCES_LIST
        # --- qrswork 核心 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork.c
        # --- 原有外设驱动 ---
        ${LIB_ROOT}/led.c
        ${LIB_ROOT}/ky.c
        ${LIB_ROOT}/rgb.c
        # --- SLE Server ---
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server/qrswork_server_adv.c
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server/qrswork_server.c
        # --- 传感器驱动 ---
        ${LIB_ROOT}/sensor/mpu6050.c
        ${LIB_ROOT}/sensor/ky039.c
        ${LIB_ROOT}/sensor/sw420.c
        ${LIB_ROOT}/sensor/max30102.c
        ${LIB_ROOT}/sensor/data_filter.c
        # --- 输出设备驱动 ---
        ${LIB_ROOT}/output/ssd1306.c
        ${LIB_ROOT}/output/ssd1306_fonts.c
        ${LIB_ROOT}/output/rgb_led.c
        ${LIB_ROOT}/output/beep.c
        ${LIB_ROOT}/output/vibration_motor.c
        # --- 算法模块 ---
        ${LIB_ROOT}/algorithm/fall_detection.c
        ${LIB_ROOT}/algorithm/breath_guide.c
        ${LIB_ROOT}/algorithm/health_alert.c
        ${LIB_ROOT}/algorithm/attitude_estimation.c
        # --- 系统底层 ---
        ${LIB_ROOT}/system/i2c_master.c
        # --- SLE 通信 ---
        ${LIB_ROOT}/comm/sle_comm.c
        ${LIB_ROOT}/comm/tcp_server.c
        # --- 健康监测主循环 ---
        ${LIB_ROOT}/app/health_monitor_main.c
        # --- 集成测试 ---
        ${LIB_ROOT}/test/mpu6050_test.c
        ${LIB_ROOT}/test/max30102_test.c
    )

    set(PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server
    )
else()
    set(SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork.c
        ${LIB_ROOT}/led.c
        ${LIB_ROOT}/ky.c
        ${LIB_ROOT}/rgb.c
        # --- 传感器驱动 ---
        ${LIB_ROOT}/sensor/mpu6050.c
        ${LIB_ROOT}/sensor/ky039.c
        ${LIB_ROOT}/sensor/sw420.c
        ${LIB_ROOT}/sensor/max30102.c
        ${LIB_ROOT}/sensor/data_filter.c
        # --- 输出设备驱动 ---
        ${LIB_ROOT}/output/ssd1306.c
        ${LIB_ROOT}/output/ssd1306_fonts.c
        ${LIB_ROOT}/output/rgb_led.c
        ${LIB_ROOT}/output/beep.c
        ${LIB_ROOT}/output/vibration_motor.c
        # --- 算法模块 ---
        ${LIB_ROOT}/algorithm/fall_detection.c
        ${LIB_ROOT}/algorithm/breath_guide.c
        ${LIB_ROOT}/algorithm/health_alert.c
        ${LIB_ROOT}/algorithm/attitude_estimation.c
        # --- 系统底层 ---
        ${LIB_ROOT}/system/i2c_master.c
        # --- SLE 通信 ---
        ${LIB_ROOT}/comm/sle_comm.c
        ${LIB_ROOT}/comm/tcp_server.c
        # --- 健康监测主循环 ---
        ${LIB_ROOT}/app/health_monitor_main.c
        # --- 集成测试 ---
        ${LIB_ROOT}/test/mpu6050_test.c
        ${LIB_ROOT}/test/max30102_test.c
    )

    set(PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/
    )
endif()

set(SOURCES "${SOURCES_LIST}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)

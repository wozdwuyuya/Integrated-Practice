#===============================================================================
# @brief    cmake file for qrswork
#===============================================================================

# ======================================================================
# 头文件搜索路径：确保 qrswork 主程序及各子模块能互相 #include
# ======================================================================
# lib/ 根目录：支持 #include "sensor/max30102.h"、#include "output/ssd1306.h" 等相对路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
# sensor/：支持 #include "max30102.h"、#include "mpu6050.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor)
# output/：支持 #include "ssd1306.h"、#include "rgb_led.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/output)
# system/：支持 #include "i2c_master.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/system)
# algorithm/：支持 #include "fall_detection.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm)
# app/：支持 #include "health_monitor_main.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/app)
# qrswork_server/：支持 #include "qrswork_server.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server)

# ======================================================================
# 源文件列表
# ======================================================================
if(DEFINED CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER)
    set(SOURCES_LIST
        # --- qrswork 核心 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork.c
        # --- 原有外设驱动 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/ky.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/rgb.c
        # --- SLE Server ---
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server/qrswork_server_adv.c
        ${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server/qrswork_server.c
        # --- [迁移] 传感器驱动 (原 lib/sensor/) ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/mpu6050.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/ky039.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/sw420.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/data_filter.c
        # --- [迁移] 输出设备驱动 (原 lib/output/) ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/ssd1306.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/ssd1306_fonts.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/rgb_led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/beep.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/vibration_motor.c
        # --- [迁移] 算法模块 (原 lib/algorithm/) ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/fall_detection.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/breath_guide.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/health_alert.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/attitude_estimation.c
        # --- [迁移] 系统底层 (原 lib/system/) ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/system/i2c_master.c
        # --- [迁移] SLE 通信 (原 lib/comm/) ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sle_comm.c
        # --- [迁移] 健康监测主循环 (原 lib/app/) ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/app/health_monitor_main.c
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
        # --- [迁移] 传感器驱动 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/mpu6050.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/ky039.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/sw420.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor/data_filter.c
        # --- [迁移] 输出设备驱动 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/ssd1306.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/ssd1306_fonts.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/rgb_led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/beep.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/output/vibration_motor.c
        # --- [迁移] 算法模块 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/fall_detection.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/breath_guide.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/health_alert.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm/attitude_estimation.c
        # --- [迁移] 系统底层 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/system/i2c_master.c
        # --- [迁移] SLE 通信 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/sle_comm.c
        # --- [迁移] 健康监测主循环 ---
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/app/health_monitor_main.c
    )

    set(PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/
    )
endif()

set(SOURCES "${SOURCES_LIST}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)

add_subdirectory_if_exist(lib)

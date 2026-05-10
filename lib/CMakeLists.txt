
# include 路径：lib 根目录（这样 #include "system/i2c_master.h" 能从任何子目录找到）
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/system/i2c_master.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor/mpu6050.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor/ky039.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor/sw420.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor/max30102.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor/data_filter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/output/ssd1306.c
    ${CMAKE_CURRENT_SOURCE_DIR}/output/ssd1306_fonts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/output/rgb_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/output/beep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/output/vibration_motor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/fall_detection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/breath_guide.c
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/health_alert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm/attitude_estimation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/comm/sle_comm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/health_monitor_main.c
)
set(SOURCES ${SOURCES_LIST} PARENT_SCOPE)

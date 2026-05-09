// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file data_filter.h
 * @brief 数据滤波算法库
 * @note 提供卡尔曼滤波和滑动平均滤波，用于传感器数据降噪
 */

#ifndef DATA_FILTER_H
#define DATA_FILTER_H

#include <stdint.h>
#include <stdbool.h>

// ========== 卡尔曼滤波器 ==========

// 一维卡尔曼滤波器结构体
typedef struct {
    float q;    // 过程噪声协方差
    float r;    // 测量噪声协方差
    float x;    // 估计值
    float p;    // 估计误差协方差
    float k;    // 卡尔曼增益
    bool initialized;
} kalman_filter_t;

// 初始化卡尔曼滤波器
void kalman_init(kalman_filter_t *kf, float q, float r, float initial_value);

// 卡尔曼滤波单次更新
float kalman_update(kalman_filter_t *kf, float measurement);

// ========== 滑动平均滤波器 ==========

// 滑动平均滤波器结构体
typedef struct {
    float *buffer;       // 数据缓冲区
    uint8_t size;        // 窗口大小
    uint8_t index;       // 当前索引
    float sum;           // 数据总和
    uint8_t count;       // 已填充数量
    bool initialized;
} moving_average_filter_t;

// 初始化滑动平均滤波器（需预分配buffer）
bool moving_average_init(moving_average_filter_t *maf, float *buffer, uint8_t size);

// 滑动平均滤波单次更新
float moving_average_update(moving_average_filter_t *maf, float new_value);

// 获取当前平均值
float moving_average_get(moving_average_filter_t *maf);

// 重置滤波器
void moving_average_reset(moving_average_filter_t *maf, float reset_value);

// ========== 便捷接口 ==========

// 三轴数据滤波结构体
typedef struct {
    kalman_filter_t kf_x;
    kalman_filter_t kf_y;
    kalman_filter_t kf_z;
} kalman_3d_t;

// 初始化三轴卡尔曼滤波器
void kalman_3d_init(kalman_3d_t *kf3d, float q, float r, float init_x, float init_y, float init_z);

// 三轴卡尔曼滤波更新
void kalman_3d_update(kalman_3d_t *kf3d, float mx, float my, float mz, float *ox, float *oy, float *oz);

#endif /* DATA_FILTER_H */

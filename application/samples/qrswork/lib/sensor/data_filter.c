/**
 * @file data_filter.c
 * @brief 数据滤波算法库实现
 * @note 实现卡尔曼滤波和滑动平均滤波
 */

#include "data_filter.h"

// ========== 卡尔曼滤波器 ==========

void kalman_init(kalman_filter_t *kf, float q, float r, float initial_value){
    kf->q = q;             // 过程噪声（越小越信任模型）
    kf->r = r;             // 测量噪声（越小越信任测量）
    kf->x = initial_value; // 初始估计值
    kf->p = 1.0f;          // 初始误差协方差
    kf->k = 0.0f;          // 卡尔曼增益
    kf->initialized = true;
}

float kalman_update(kalman_filter_t *kf, float measurement){
    if(!kf->initialized) {
        kf->x = measurement;
        kf->initialized = true;
        return measurement;
    }

    // 预测步骤
    kf->p = kf->p + kf->q;

    // 更新步骤
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1.0f - kf->k) * kf->p;

    return kf->x;
}

// ========== 滑动平均滤波器 ==========

bool moving_average_init(moving_average_filter_t *maf, float *buffer, uint8_t size){
    if(buffer == NULL || size == 0) return false;

    maf->buffer = buffer;
    maf->size = size;
    maf->index = 0;
    maf->sum = 0;
    maf->count = 0;
    maf->initialized = true;

    // 清零缓冲区
    for(int i = 0; i < size; i++) {
        buffer[i] = 0;
    }

    return true;
}

float moving_average_update(moving_average_filter_t *maf, float new_value){
    if(!maf->initialized || maf->buffer == NULL) return new_value;

    // 如果缓冲区未满，累加
    if(maf->count < maf->size) {
        maf->sum += new_value;
        maf->buffer[maf->index] = new_value;
        maf->count++;
    } else {
        // 缓冲区已满，替换最旧数据
        maf->sum -= maf->buffer[maf->index];
        maf->sum += new_value;
        maf->buffer[maf->index] = new_value;
    }

    maf->index = (maf->index + 1) % maf->size;

    return maf->sum / maf->count;
}

float moving_average_get(moving_average_filter_t *maf){
    if(!maf->initialized || maf->count == 0) return 0;
    return maf->sum / maf->count;
}

void moving_average_reset(moving_average_filter_t *maf, float reset_value){
    if(!maf->initialized) return;

    maf->sum = 0;
    maf->count = 0;
    maf->index = 0;

    for(int i = 0; i < maf->size; i++) {
        maf->buffer[i] = reset_value;
    }
}

// ========== 三轴卡尔曼滤波 ==========

void kalman_3d_init(kalman_3d_t *kf3d, float q, float r, float init_x, float init_y, float init_z){
    kalman_init(&kf3d->kf_x, q, r, init_x);
    kalman_init(&kf3d->kf_y, q, r, init_y);
    kalman_init(&kf3d->kf_z, q, r, init_z);
}

void kalman_3d_update(kalman_3d_t *kf3d, float mx, float my, float mz, float *ox, float *oy, float *oz){
    if(ox) *ox = kalman_update(&kf3d->kf_x, mx);
    if(oy) *oy = kalman_update(&kf3d->kf_y, my);
    if(oz) *oz = kalman_update(&kf3d->kf_z, mz);
}

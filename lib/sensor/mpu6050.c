/**
 * @file mpu6050.c
 * @brief MPU6050六轴IMU传感器驱动实现
 * @note 使用I2C接口，与OLED共用总线
 */

#include "mpu6050.h"
#include "system/i2c_master.h"
#include "errcode.h"
#include "sensor/data_filter.h"
#include "osal_debug.h"

#define MPU6050_I2C_BUS 1  // 使用I2C总线1

// 当前量程设置（用于数据转换）
static float accel_scale = 1.0f;
static float gyro_scale = 1.0f;

// 卡尔曼滤波器（降噪）
static kalman_3d_t accel_kf;   // 加速度滤波
static kalman_3d_t gyro_kf;    // 陀螺仪滤波
static bool filter_initialized = false;

// I2C写单字节
static errcode_t mpu6050_i2c_write(uint8_t reg, uint8_t val){
    uint8_t buff[2] = {reg, val};
    i2c_data_t data = {0};
    errcode_t ret;

    data.send_buf = buff;
    data.send_len = 2;
    if(!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_write(MPU6050_I2C_BUS, MPU6050_ADDRESS, &data);
    i2c_master_unlock();
    if(ret != ERRCODE_SUCC) {
        osal_printk("MPU6050 write reg 0x%02X failed, ret:0x%X\r\n", reg, ret);
    }
    return ret;
}

// I2C读单字节
static errcode_t mpu6050_i2c_read(uint8_t reg, uint8_t *re_data){
    i2c_data_t data = {0};
    errcode_t ret;

    if(re_data == NULL) return 1;

    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = re_data;
    data.receive_len = 1;
    if(!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_read(MPU6050_I2C_BUS, MPU6050_ADDRESS, &data);
    i2c_master_unlock();
    if(ret != ERRCODE_SUCC) {
        osal_printk("MPU6050 read reg 0x%02X failed, ret:0x%X\r\n", reg, ret);
    }
    return ret;
}

// I2C读多字节
static errcode_t mpu6050_i2c_read_buf(uint8_t reg, uint8_t *buf, uint8_t len){
    i2c_data_t data = {0};
    errcode_t ret;

    if(buf == NULL || len == 0) return 1;

    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = buf;
    data.receive_len = len;
    if(!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_writeread(MPU6050_I2C_BUS, MPU6050_ADDRESS, &data);
    i2c_master_unlock();
    return ret;
}

// 检查MPU6050是否在线
bool mpu6050_check_id(void){
    uint8_t id = 0;
    if(mpu6050_i2c_read(MPU6050_REG_WHO_AM_I, &id) != ERRCODE_SUCC) {
        return false;
    }
    osal_printk("MPU6050 WHO_AM_I: 0x%02X (expected 0x68)\r\n", id);
    return (id == 0x68);
}

// 软复位
void mpu6050_reset(void){
    mpu6050_i2c_write(MPU6050_REG_PWR_MGMT_1, 0x80);
}

// 设置加速度量程
static void mpu6050_set_accel_range(uint8_t range){
    mpu6050_i2c_write(MPU6050_REG_ACCEL_CONFIG, range);
    switch(range) {
        case MPU6050_ACCEL_RANGE_2G:  accel_scale = 16384.0f; break;
        case MPU6050_ACCEL_RANGE_4G:  accel_scale = 8192.0f;  break;
        case MPU6050_ACCEL_RANGE_8G:  accel_scale = 4096.0f;  break;
        case MPU6050_ACCEL_RANGE_16G: accel_scale = 2048.0f;  break;
        default: accel_scale = 16384.0f; break;
    }
}

// 设置陀螺仪量程
static void mpu6050_set_gyro_range(uint8_t range){
    mpu6050_i2c_write(MPU6050_REG_GYRO_CONFIG, range);
    switch(range) {
        case MPU6050_GYRO_RANGE_250:  gyro_scale = 131.0f;   break;
        case MPU6050_GYRO_RANGE_500:  gyro_scale = 65.5f;    break;
        case MPU6050_GYRO_RANGE_1000: gyro_scale = 32.8f;    break;
        case MPU6050_GYRO_RANGE_2000: gyro_scale = 16.4f;    break;
        default: gyro_scale = 131.0f; break;
    }
}

// MPU6050初始化
bool mpu6050_init(uint8_t accel_range, uint8_t gyro_range){
    // 检查设备ID
    if(!mpu6050_check_id()) {
        osal_printk("MPU6050 not found!\r\n");
        return false;
    }

    // 软复位
    mpu6050_reset();

    // 等待复位完成
    osDelay(100);

    // 唤醒设备，选择时钟源（PLL with X-axis gyro）
    mpu6050_i2c_write(MPU6050_REG_PWR_MGMT_1, 0x01);

    // 设置采样率（1kHz / (1+9) = 100Hz）
    mpu6050_i2c_write(MPU6050_REG_SMPLRT_DIV, 0x09);

    // 设置DLPF（数字低通滤波器）
    mpu6050_i2c_write(MPU6050_REG_CONFIG, 0x03);

    // 设置量程
    mpu6050_set_accel_range(accel_range);
    mpu6050_set_gyro_range(gyro_range);

    osal_printk("MPU6050 init success\r\n");
    return true;
}

// 读取原始数据
bool mpu6050_read_raw(mpu6050_data_t *data){
    uint8_t buf[14];  // 加速度6 + 温度2 + 陀螺仪6 = 14字节

    if(data == NULL) return false;

    // 从加速度X轴高字节开始连续读取14字节
    if(mpu6050_i2c_read_buf(MPU6050_REG_ACCEL_XOUT_H, buf, 14) != ERRCODE_SUCC) {
        return false;
    }

    // 解析加速度数据
    data->accel.x = (int16_t)((buf[0] << 8) | buf[1]);
    data->accel.y = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel.z = (int16_t)((buf[4] << 8) | buf[5]);

    // 解析温度数据
    int16_t temp_raw = (int16_t)((buf[6] << 8) | buf[7]);
    data->temperature = temp_raw / 340.0f + 36.53f;

    // 解析陀螺仪数据
    data->gyro.x = (int16_t)((buf[8] << 8) | buf[9]);
    data->gyro.y = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro.z = (int16_t)((buf[12] << 8) | buf[13]);

    return true;
}

// 读取处理后的数据（g值和°/s）
bool mpu6050_read_processed(mpu6050_data_t *data){
    if(!mpu6050_read_raw(data)) {
        return false;
    }

    // 首次使用时初始化卡尔曼滤波器
    if(!filter_initialized) {
        kalman_3d_init(&accel_kf, 0.01f, 0.5f, 0, 0, 1.0f);
        kalman_3d_init(&gyro_kf, 0.01f, 0.3f, 0, 0, 0);
        filter_initialized = true;
    }

    // 转换为g值
    float raw_accel[3];
    raw_accel[0] = data->accel.x / accel_scale;
    raw_accel[1] = data->accel.y / accel_scale;
    raw_accel[2] = data->accel.z / accel_scale;

    // 卡尔曼滤波降噪
    kalman_3d_update(&accel_kf, raw_accel[0], raw_accel[1], raw_accel[2],
                     &data->accel_g[0], &data->accel_g[1], &data->accel_g[2]);

    // 转换为°/s
    float raw_gyro[3];
    raw_gyro[0] = data->gyro.x / gyro_scale;
    raw_gyro[1] = data->gyro.y / gyro_scale;
    raw_gyro[2] = data->gyro.z / gyro_scale;

    // 卡尔曼滤波降噪
    kalman_3d_update(&gyro_kf, raw_gyro[0], raw_gyro[1], raw_gyro[2],
                     &data->gyro_dps[0], &data->gyro_dps[1], &data->gyro_dps[2]);

    return true;
}

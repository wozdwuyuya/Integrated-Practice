/**
 * @file mpu6050_test.h
 * @brief MPU6050 集成联调测试任务
 * @note  用于验证 MPU6050 在真实硬件上的 I2C 通信和数据读取
 */

#ifndef MPU6050_TEST_H
#define MPU6050_TEST_H

/**
 * @brief MPU6050 联调测试主函数（阻塞，内部含死循环）
 * @note  调用后会初始化 I2C 总线 + MPU6050，然后持续打印传感器数据
 *        适合作为独立线程入口，或替换 app_main 中的主循环进行联调
 */
void mpu6050_test_run(void);

#endif // MPU6050_TEST_H

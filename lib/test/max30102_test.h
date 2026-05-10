/**
 * @file max30102_test.h
 * @brief MAX30102 心率血氧传感器集成联调测试任务
 * @note  用于验证 MAX30102 在真实硬件上的 I2C 通信和心率血氧数据输出
 */

#ifndef MAX30102_TEST_H
#define MAX30102_TEST_H

/**
 * @brief MAX30102 联调测试主函数（阻塞，内部含死循环）
 * @note  调用后会初始化 I2C 总线 + MAX30102，然后持续打印心率血氧数据
 *        适合作为独立线程入口，或替换 app_main 中的主循环进行联调
 */
void max30102_test_run(void);

#endif // MAX30102_TEST_H

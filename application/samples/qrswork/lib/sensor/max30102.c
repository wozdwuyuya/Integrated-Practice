/**
 * @file max30102.c
 * @brief MAX30102心率血氧传感器驱动实现
 * @note [四柱-监测] 心率加权平均修复 + 代码去重：复用 data_filter
 * @note 实现传感器初始化、FIFO数据读取、心率血氧算法
 */

#include "max30102.h"
#define max30102_address_ID 0x57  // MAX30102 I2C设备地址
#include "system/i2c_master.h"
#include "sensor/data_filter.h"
#include "osal_debug.h"
#define max30102_I2C_BUS 1  // 使用I2C总线1

uint32_t return_ac[2] = {0};  // 输出数组：[0]心率, [1]血氧

#define MAX30102_REG_INTR_STATUS1  0x00
#define MAX30102_REG_INTR_STATUS2  0x01
#define MAX30102_REG_FIFO_WR_PTR   0x04
#define MAX30102_REG_FIFO_RD_PTR   0x06
#define MAX30102_REG_FIFO_DATA     0x07
#define MAX30102_REG_MODE_CONFIG   0x09
#define MAX30102_REG_SPO2_CONFIG   0x0A
#define MAX30102_REG_LED1_PA       0x0C
#define MAX30102_REG_LED2_PA       0x0D
#define MAX30102_REG_PART_ID       0xFF

#define MAX30102_MODE_SPO2         0x03


#define RATE_HZ 100
#define INTERVAL_US 10000
// 系统时间相关变量
static volatile uint32_t system_tick_ms = 0;  // 系统时间计数(ms)
static uint32_t last_time = 0;                // 上次采样时间
static timer_handle_t timer = NULL;           // 定时器句柄
static timer_handle_t hr_timer = NULL;        // 心率定时器句柄
static uint8_t ready = 0;                     // 数据就绪标志

// 定时器回调：每10ms触发一次
static void timer_callback(uintptr_t data){
    system_tick_ms += 10;
    ready = 1;
    uapi_timer_start(timer,INTERVAL_US,timer_callback,0);
}

// 获取系统时间(ms)
uint32_t get_time_ms(void){
    return system_tick_ms;
}

// 定时器初始化：配置10ms周期定时器
bool timer_init(void){
    errcode_t ret;
    ret = uapi_timer_init();
    if(ret != ERRCODE_SUCC){
        return false;
    }
    ret = uapi_timer_adapter(TIMER_INDEX_0,0,1);
    if(ret != ERRCODE_SUCC){
        return false;
    }
    ret = uapi_timer_create(TIMER_INDEX_0,&timer);
    if(ret != ERRCODE_SUCC){
        return false;
    }
    ret = uapi_timer_start(timer,INTERVAL_US,timer_callback,0);
    if(ret != ERRCODE_SUCC){
        return false;
    }
    system_tick_ms = 0;
    ready = 0;
    return true;
}

// 获取高精度时间(us)
uint32_t get_high_time_us(void){
    uint32_t current_time_us = 0;
    uapi_timer_get_current_time_us(TIMER_INDEX_0,&current_time_us);
    return current_time_us;
}

// 检查是否可以采样(10ms间隔)
bool is_time(void){
    uint32_t current_time = get_time_ms();
    uint32_t elapsed = current_time - last_time;
    if(ready && elapsed >= 10){
        last_time = current_time;
        ready = 0;
        return true;
    }
    return false;
}

// 定时器清理
void clean_timer(void){
    if(timer != NULL){
        uapi_timer_stop(TIMER_INDEX_0);
        uapi_timer_delete(TIMER_INDEX_0);
        timer = NULL;
    }
    uapi_timer_deinit();
}


// I2C写单字节：向指定寄存器写入一个字节
static errcode_t max30102_i2c_write(uint8_t reg,uint8_t val){
    uint8_t buff[2]={reg,val};
    i2c_data_t data ={0};
    errcode_t ret;

    data.send_buf = buff;
    data.send_len = 2;
    if(!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_write(max30102_I2C_BUS,max30102_address_ID,&data);
    i2c_master_unlock();
    if(ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 write reg 0x%02X failed, ret:0x%X\r\n", reg, ret);
    }

    return ret;
}

// I2C读单字节：从指定寄存器读取一个字节
static errcode_t max30102_i2c_read(uint8_t reg, uint8_t *re_data){
    i2c_data_t data = {0};
    errcode_t ret;

    if(re_data == NULL) return 1;

    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = re_data;
    data.receive_len =1;
    if(!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_read(max30102_I2C_BUS,max30102_address_ID,&data);
    i2c_master_unlock();
    if(ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 read reg 0x%02X failed, ret:0x%X\r\n", reg, ret);
    }

    return ret;
}

// I2C读多字节：从指定寄存器连续读取多个字节
static errcode_t max30102_i2c_read_buf(uint8_t reg,uint8_t *buf,uint8_t len){
    i2c_data_t data = {0};
    errcode_t ret;

    if(buf == NULL || len == 0) return 1;

    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = buf;
    data.receive_len = len;
    if(!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_writeread(max30102_I2C_BUS,max30102_address_ID,&data);
    i2c_master_unlock();

    return ret;
}

// MAX30102传感器初始化：验证ID、软件复位、配置寄存器、启动定时器
bool max30102_init(void){
    uint8_t part_id = 0;
    uint8_t flag = 0;
    uint32_t retry = 0;

    // 第一步：先读器件ID，确认I2C通信正常，这是最可靠的验证方式
    osDelay(50); // 上电稳定延时
    if(max30102_i2c_read(MAX30102_REG_PART_ID, &part_id) != ERRCODE_SUCC) {
        osal_printk("MAX30102 read ID failed\r\n");
        return false;
    }
    osal_printk("MAX30102 part ID: 0x%02X (expected 0x15)\r\n", part_id);
    if(part_id != 0x15) { // MAX30102固定器件ID为0x15
        osal_printk("MAX30102 wrong chip ID\r\n");
        return false;
    }

    // 第二步：软件复位
    if(max30102_i2c_write(MAX30102_REG_MODE_CONFIG, 0x40) != ERRCODE_SUCC) {
        return false;
    }
    // 复位等待，必须加延时，不能空循环
    osDelay(20);
    retry = 0;
    while(retry < 100) {
        if(max30102_i2c_read(MAX30102_REG_MODE_CONFIG, &flag) != ERRCODE_SUCC) {
            retry++;
            osDelay(1);
            continue;
        }
        if(!(flag & 0x40)) break;
        retry++;
        osDelay(1);
    }
    if(retry >= 100) {
        osal_printk("MAX30102 reset timeout\r\n");
        return false;
    }
    osDelay(10);

    // 第三步：寄存器配置
    max30102_i2c_write(MAX30102_REG_FIFO_WR_PTR, 0x00);
    max30102_i2c_write(MAX30102_REG_FIFO_RD_PTR, 0x00);
    max30102_i2c_write(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    max30102_i2c_write(MAX30102_REG_SPO2_CONFIG, 0x27);
    max30102_i2c_write(MAX30102_REG_LED1_PA, 0x1f);
    max30102_i2c_write(MAX30102_REG_LED2_PA, 0x1f);
    osDelay(10);

    // 第四步：定时器初始化
    if(!timer_init()){
        return false;
    }
    return true;
}


// 读取FIFO数据：获取红光和红外光原始值
void max30102_read_fifo(uint32_t *red,uint32_t *ir){
    uint8_t buf[6]={0};
    max30102_i2c_read_buf(MAX30102_REG_FIFO_DATA,buf,6);
    *red = ((uint32_t)buf[0] << 16)|((uint32_t)buf[1]<<8)|buf[2];
    *ir = ((uint32_t)buf[3]<<16)|((uint32_t)buf[4]<<8)|buf[5];
    *red >>= 6;  // 右移6位取有效数据
    *ir >>= 6;
}

// 缓冲区和窗口大小定义
#define BUFFER_SIZE 256      // 原始数据缓冲区大小
#define HR_WINDOW_SIZE 10    // 心率计算窗口大小
#define SPO2_WINDOW_SIZE 5   // 血氧计算窗口大小

// 传感器数据结构
typedef struct{
    uint32_t red_buffer[BUFFER_SIZE];  // 红光缓冲区
    uint32_t ir_buffer[BUFFER_SIZE];   // 红外光缓冲区
    int buffer_index;                  // 缓冲区索引
    uint32_t dc_red;                   // 红光直流分量
    uint32_t dc_ir;                    // 红外直流分量
    uint32_t ac_red;                   // 红光交流分量
    uint32_t ac_ir;                    // 红外交流分量
}max30102_data_t;

// 生命体征数据结构
typedef struct{
    int heart_rate;          // 心率值
    int spo2;                // 血氧值
    uint8_t heart_rate_enable;  // 心率有效标志
    uint8_t spo2_enable;        // 血氧有效标志
}lifeavaiable_data_t;

static max30102_data_t sensor_data = {0};      // 传感器数据
static lifeavaiable_data_t vital_signs = {0};  // 生命体征
static uint32_t hr_samples[HR_WINDOW_SIZE];     // 心率采样缓冲
static uint8_t hr_sample_index = 0;             // 心率采样索引

// PPG信号滑动平均滤波（降噪，复用 data_filter.h）
#define PPG_FILTER_SIZE 4
static float red_filter_buf[PPG_FILTER_SIZE];
static float ir_filter_buf[PPG_FILTER_SIZE];
static moving_average_filter_t red_filter;
static moving_average_filter_t ir_filter;
static bool ppg_filter_init = false;

// 计算AC/DC分量：分离交流和直流信号
void calculate_ac_components(uint32_t red, uint32_t ir) {
    static uint32_t red_sum = 0;
    static uint32_t ir_sum = 0;

    // 首次调用初始化滑动平均滤波器
    if(!ppg_filter_init) {
        moving_average_init(&red_filter, red_filter_buf, PPG_FILTER_SIZE);
        moving_average_init(&ir_filter, ir_filter_buf, PPG_FILTER_SIZE);
        ppg_filter_init = true;
    }

    // 滑动平均滤波降噪
    float filtered_red = moving_average_update(&red_filter, (float)red);
    float filtered_ir = moving_average_update(&ir_filter, (float)ir);

    // 一阶低通滤波计算直流分量
    red_sum = (red_sum * 0.95) + (filtered_red * 0.05);
    ir_sum = (ir_sum * 0.95) + (filtered_ir * 0.05);

    sensor_data.dc_red = red_sum;
    sensor_data.dc_ir = ir_sum;

    // 交流分量 = 滤波后信号 - 直流分量
    if (filtered_red > sensor_data.dc_red) {
        sensor_data.ac_red = filtered_red - sensor_data.dc_red;
    } else {
        sensor_data.ac_red = 0;
    }

    if (filtered_ir > sensor_data.dc_ir) {
        sensor_data.ac_ir = filtered_ir - sensor_data.dc_ir;
    } else {
        sensor_data.ac_ir = 0;
    }

    // 存储滤波后数据到缓冲区
    sensor_data.red_buffer[sensor_data.buffer_index]=(uint32_t)filtered_red;
    sensor_data.ir_buffer[sensor_data.buffer_index]=(uint32_t)filtered_ir;
    sensor_data.buffer_index = (sensor_data.buffer_index + 1)%BUFFER_SIZE;
}

// 心率检测算法：基于峰值检测计算心率（优化版）
// 改进点：1.自适应阈值 2.信号质量评估 3.异常值过滤
void heart_rate_enable_in(void){
    static uint32_t last_peak_time = 0;
    static uint32_t last_peak_value = 0;
    static int peak_count = 0;
    static uint32_t peak_intervals[5] = {0};
    static uint8_t interval_index = 0;
    static uint32_t signal_sum = 0;      // 信号累加（用于自适应阈值）
    static uint32_t signal_count = 0;    // 信号计数
    static uint32_t adaptive_threshold = 30;  // 自适应阈值

    uint32_t current_time = get_time_ms();

    // 更新信号统计（用于自适应阈值）
    signal_sum += sensor_data.ac_ir;
    signal_count++;
    if(signal_count >= 100) {
        // 每100个采样点更新一次阈值
        adaptive_threshold = (signal_sum / signal_count) * 0.6;  // 阈值为平均值的60%
        if(adaptive_threshold < 20) adaptive_threshold = 20;     // 最小阈值
        if(adaptive_threshold > 100) adaptive_threshold = 100;   // 最大阈值
        signal_sum = 0;
        signal_count = 0;
    }

    // 使用自适应阈值进行峰值检测
    if(sensor_data.ac_ir > adaptive_threshold) {
        if(sensor_data.ac_ir > last_peak_value) {
            last_peak_value = sensor_data.ac_ir;
        }
        else if(last_peak_value > 0 && sensor_data.ac_ir < last_peak_value * 0.75) {
            // 检测到峰值下降（从75%下降）
            uint32_t current_interval = current_time - last_peak_time;

            // 验证峰值间隔（300ms-2000ms对应30-200BPM）
            if(current_interval > 300 && current_interval < 2000) {
                peak_intervals[interval_index] = current_interval;
                interval_index = (interval_index + 1) % 5;
                peak_count++;

                if(peak_count >= 2) {
                    // 计算平均间隔
                    uint32_t avg_interval = 0;
                    int valid_interval = 0;

                    for(int i = 0; i < 5; i++) {
                        if(peak_intervals[i] > 0) {
                            avg_interval += peak_intervals[i];
                            valid_interval++;
                        }
                    }

                    if(valid_interval > 0) {
                        avg_interval = avg_interval / valid_interval;
                        int hr = 60000 / avg_interval;  // 转换为BPM

                        // 验证心率范围并过滤异常值
                        if(hr >= 40 && hr <= 200) {
                            // 检查心率变化是否合理（单次变化不超过20%）
                            if(vital_signs.heart_rate_enable && vital_signs.heart_rate > 0) {
                                int hr_diff = hr - vital_signs.heart_rate;
                                if(hr_diff < 0) hr_diff = -hr_diff;
                                if(hr_diff > vital_signs.heart_rate * 0.2) {
                                    // 变化过大，可能是噪声，忽略这次
                                    last_peak_value = 0;
                                    last_peak_time = current_time;
                                    return;
                                }
                            }

                            hr_samples[hr_sample_index] = hr;
                            hr_sample_index = (hr_sample_index + 1) % HR_WINDOW_SIZE;

                            // 计算加权平均心率（近期数据权重更高）
                            // [修复] hr_sample_index 已递增，指向最旧槽位
                            // 最新样本在 (hr_sample_index - 1 + SIZE) % SIZE
                            uint32_t hr_sum = 0;
                            uint32_t weight_sum = 0;
                            int valid_hr_samples = 0;
                            uint8_t newest_idx = (hr_sample_index + HR_WINDOW_SIZE - 1) % HR_WINDOW_SIZE;

                            for(int i = 0; i < HR_WINDOW_SIZE; i++) {
                                if(hr_samples[i] > 0) {
                                    // 权重：距最新样本越近权重越高
                                    uint32_t age = (newest_idx + HR_WINDOW_SIZE - i) % HR_WINDOW_SIZE;
                                    uint32_t weight = HR_WINDOW_SIZE - age;  // 最新=SIZE, 最旧=1
                                    hr_sum += hr_samples[i] * weight;
                                    weight_sum += weight;
                                    valid_hr_samples++;
                                }
                            }

                            if(valid_hr_samples > 0 && weight_sum > 0) {
                                vital_signs.heart_rate = hr_sum / weight_sum;
                                vital_signs.heart_rate_enable = 1;
                            }
                        }
                    }
                }
            }
            last_peak_value = 0;
            last_peak_time = current_time;
        }
    }
}


// 血氧计算：基于红光和红外光比值计算SpO2
void calculate_spo2(void){
    if(sensor_data.dc_red > 0 && sensor_data.dc_ir > 0){
        float red_ratio = (float)sensor_data.ac_red / sensor_data.dc_red;
        float ir_ratio = (float)sensor_data.ac_ir / sensor_data.dc_ir;

        if(ir_ratio > 0){
            float R = red_ratio / ir_ratio;

            float spo2_float = 110.0f -25.0f * R;

            if(spo2_float < 70.0f)spo2_float = 70.0f;
            if(spo2_float > 100.0f)spo2_float = 100.0f;
            vital_signs.spo2 = (int)spo2_float;
            vital_signs.spo2_enable = 1;
        }   
    }

}

// 主数据处理函数：读取传感器数据并计算心率血氧
bool main_max30102_data(void){
    uint32_t red,ir;

    max30102_read_fifo(&red,&ir);
    calculate_ac_components(red,ir);
    heart_rate_enable_in();
    calculate_spo2();
    if(vital_signs.heart_rate_enable && vital_signs.spo2_enable){
        return_ac[0] = vital_signs.heart_rate;
        return_ac[1] = vital_signs.spo2;
        return true;
    }
    return false;
}

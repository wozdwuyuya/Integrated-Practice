/**
 * @file max30102.c
 * @brief MAX30102心率血氧传感器驱动实现
 * @note 实现传感器初始化、FIFO数据读取、心率血氧算法
 * @note 复用 data_filter.h 中的滑动平均滤波器
 */

#include "max30102.h"
#include "system/i2c_master.h"
#include "sensor/data_filter.h"
#include "osal_debug.h"

uint32_t return_ac[2] = {0};

// ========== 定时器配置 ==========
#define SAMPLE_INTERVAL_US   10000   // 10ms采样间隔
#define SAMPLE_INTERVAL_MS   10

// 系统时间相关变量
static volatile uint32_t system_tick_ms = 0;
static uint32_t last_time = 0;
static timer_handle_t timer = NULL;
static uint8_t ready = 0;

// 定时器回调：每10ms触发一次
static void timer_callback(uintptr_t data)
{
    system_tick_ms += SAMPLE_INTERVAL_MS;
    ready = 1;
    uapi_timer_start(timer, SAMPLE_INTERVAL_US, timer_callback, 0);
}

// 获取系统时间(ms)
static uint32_t get_time_ms(void)
{
    return system_tick_ms;
}

// 定时器初始化：配置10ms周期定时器
static bool timer_init(void)
{
    errcode_t ret;

    ret = uapi_timer_init();
    if (ret != ERRCODE_SUCC) {
        return false;
    }
    ret = uapi_timer_adapter(TIMER_INDEX_0, 0, 1);
    if (ret != ERRCODE_SUCC) {
        return false;
    }
    ret = uapi_timer_create(TIMER_INDEX_0, &timer);
    if (ret != ERRCODE_SUCC) {
        return false;
    }
    ret = uapi_timer_start(timer, SAMPLE_INTERVAL_US, timer_callback, 0);
    if (ret != ERRCODE_SUCC) {
        return false;
    }
    system_tick_ms = 0;
    ready = 0;
    return true;
}

bool is_time(void)
{
    uint32_t current_time = get_time_ms();
    uint32_t elapsed = current_time - last_time;

    if (ready && elapsed >= SAMPLE_INTERVAL_MS) {
        last_time = current_time;
        ready = 0;
        return true;
    }
    return false;
}

void clean_timer(void)
{
    if (timer != NULL) {
        uapi_timer_stop(TIMER_INDEX_0);
        uapi_timer_delete(TIMER_INDEX_0);
        timer = NULL;
    }
    uapi_timer_deinit();
}

// ========== I2C通信 ==========
// I2C写单字节
static errcode_t max30102_i2c_write(uint8_t reg, uint8_t val)
{
    uint8_t buff[2] = {reg, val};
    i2c_data_t data = {0};
    errcode_t ret;

    data.send_buf = buff;
    data.send_len = 2;
    if (!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_write(MAX30102_I2C_BUS, MAX30102_ADDRESS, &data);
    i2c_master_unlock();
    if (ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 write reg 0x%02X failed, ret:0x%X\r\n", reg, ret);
    }
    return ret;
}

// I2C读单字节
static errcode_t max30102_i2c_read(uint8_t reg, uint8_t *re_data)
{
    i2c_data_t data = {0};
    errcode_t ret;

    if (re_data == NULL) return 1;

    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = re_data;
    data.receive_len = 1;
    if (!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_read(MAX30102_I2C_BUS, MAX30102_ADDRESS, &data);
    i2c_master_unlock();
    if (ret != ERRCODE_SUCC) {
        osal_printk("MAX30102 read reg 0x%02X failed, ret:0x%X\r\n", reg, ret);
    }
    return ret;
}

// I2C读多字节
static errcode_t max30102_i2c_read_buf(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_data_t data = {0};
    errcode_t ret;

    if (buf == NULL || len == 0) return 1;

    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = buf;
    data.receive_len = len;
    if (!i2c_master_lock()) return 1;
    ret = uapi_i2c_master_writeread(MAX30102_I2C_BUS, MAX30102_ADDRESS, &data);
    i2c_master_unlock();
    return ret;
}

// ========== MAX30102初始化 ==========
#define MAX30102_RESET_TIMEOUT   100
#define MAX30102_POWERUP_DELAY   50
#define MAX30102_RESET_DELAY     20
#define MAX30102_CONFIG_DELAY    10

bool max30102_init(void)
{
    uint8_t part_id = 0;
    uint8_t flag = 0;
    uint32_t retry = 0;

    osDelay(MAX30102_POWERUP_DELAY);
    if (max30102_i2c_read(MAX30102_REG_PART_ID_REG, &part_id) != ERRCODE_SUCC) {
        osal_printk("MAX30102 read ID failed\r\n");
        return false;
    }
    osal_printk("MAX30102 part ID: 0x%02X (expected 0x%02X)\r\n", part_id, MAX30102_PART_ID);
    if (part_id != MAX30102_PART_ID) {
        osal_printk("MAX30102 wrong chip ID\r\n");
        return false;
    }

    // 软件复位
    if (max30102_i2c_write(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET) != ERRCODE_SUCC) {
        return false;
    }
    osDelay(MAX30102_RESET_DELAY);
    while (retry < MAX30102_RESET_TIMEOUT) {
        if (max30102_i2c_read(MAX30102_REG_MODE_CONFIG, &flag) != ERRCODE_SUCC) {
            retry++;
            osDelay(1);
            continue;
        }
        if (!(flag & MAX30102_MODE_RESET)) break;
        retry++;
        osDelay(1);
    }
    if (retry >= MAX30102_RESET_TIMEOUT) {
        osal_printk("MAX30102 reset timeout\r\n");
        return false;
    }
    osDelay(MAX30102_CONFIG_DELAY);

    // 寄存器配置
    max30102_i2c_write(MAX30102_REG_FIFO_WR_PTR, 0x00);
    max30102_i2c_write(MAX30102_REG_FIFO_RD_PTR, 0x00);
    max30102_i2c_write(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    max30102_i2c_write(MAX30102_REG_SPO2_CONFIG, MAX30102_SPO2_CONFIG_VAL);
    max30102_i2c_write(MAX30102_REG_LED1_PA, MAX30102_LED_CURRENT_DEFAULT);
    max30102_i2c_write(MAX30102_REG_LED2_PA, MAX30102_LED_CURRENT_DEFAULT);
    osDelay(MAX30102_CONFIG_DELAY);

    if (!timer_init()) {
        return false;
    }
    return true;
}

// ========== FIFO数据读取 ==========
static void max30102_read_fifo(uint32_t *red, uint32_t *ir)
{
    uint8_t buf[6] = {0};

    max30102_i2c_read_buf(MAX30102_REG_FIFO_DATA, buf, 6);
    *red = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    *ir  = ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    *red >>= 6;
    *ir  >>= 6;
}

// ========== 信号处理算法 ==========
#define BUFFER_SIZE      256
#define HR_WINDOW_SIZE   10
#define PPG_FILTER_SIZE  4

// 低通滤波系数
#define LP_ALPHA_DC      0.95f
#define LP_ALPHA_AC      0.05f

// 心率检测参数
#define HR_THRESHOLD_MIN         20
#define HR_THRESHOLD_MAX         100
#define HR_THRESHOLD_RATIO       0.6f
#define HR_PEAK_DROP_RATIO       0.75f
#define HR_CHANGE_LIMIT_RATIO    0.2f
#define HR_BPM_MIN               40
#define HR_BPM_MAX               200
#define HR_INTERVAL_MIN_MS       300
#define HR_INTERVAL_MAX_MS       2000
#define HR_PEAK_INTERVAL_COUNT   5
#define HR_ADAPTIVE_SAMPLE_COUNT 100

// SpO2计算参数
#define SPO2_MIN                 70
#define SPO2_MAX                 100
#define SPO2_OFFSET              110.0f
#define SPO2_SLOPE               25.0f

typedef struct {
    uint32_t red_buffer[BUFFER_SIZE];
    uint32_t ir_buffer[BUFFER_SIZE];
    int buffer_index;
    uint32_t dc_red;
    uint32_t dc_ir;
    uint32_t ac_red;
    uint32_t ac_ir;
} max30102_data_t;

typedef struct {
    int heart_rate;
    int spo2;
    uint8_t heart_rate_enable;
    uint8_t spo2_enable;
} vital_signs_t;

static max30102_data_t sensor_data = {0};
static vital_signs_t vital_signs = {0};
static uint32_t hr_samples[HR_WINDOW_SIZE];
static uint8_t hr_sample_index = 0;

// PPG信号滑动平均滤波
static float red_filter_buf[PPG_FILTER_SIZE];
static float ir_filter_buf[PPG_FILTER_SIZE];
static moving_average_filter_t red_filter;
static moving_average_filter_t ir_filter;
static bool ppg_filter_init = false;

// 计算AC/DC分量：分离交流和直流信号
static void calculate_ac_components(uint32_t red, uint32_t ir)
{
    static uint32_t red_sum = 0;
    static uint32_t ir_sum = 0;

    if (!ppg_filter_init) {
        moving_average_init(&red_filter, red_filter_buf, PPG_FILTER_SIZE);
        moving_average_init(&ir_filter, ir_filter_buf, PPG_FILTER_SIZE);
        ppg_filter_init = true;
    }

    float filtered_red = moving_average_update(&red_filter, (float)red);
    float filtered_ir = moving_average_update(&ir_filter, (float)ir);

    red_sum = (uint32_t)(red_sum * LP_ALPHA_DC + filtered_red * LP_ALPHA_AC);
    ir_sum  = (uint32_t)(ir_sum  * LP_ALPHA_DC + filtered_ir  * LP_ALPHA_AC);

    sensor_data.dc_red = red_sum;
    sensor_data.dc_ir  = ir_sum;

    sensor_data.ac_red = (filtered_red > red_sum) ? (uint32_t)(filtered_red - red_sum) : 0;
    sensor_data.ac_ir  = (filtered_ir  > ir_sum)  ? (uint32_t)(filtered_ir  - ir_sum)  : 0;

    sensor_data.red_buffer[sensor_data.buffer_index] = (uint32_t)filtered_red;
    sensor_data.ir_buffer[sensor_data.buffer_index]  = (uint32_t)filtered_ir;
    sensor_data.buffer_index = (sensor_data.buffer_index + 1) % BUFFER_SIZE;
}

// 心率检测：基于自适应阈值的峰值检测 + 加权平均
static void detect_heart_rate(void)
{
    static uint32_t last_peak_time = 0;
    static uint32_t last_peak_value = 0;
    static int peak_count = 0;
    static uint32_t peak_intervals[HR_PEAK_INTERVAL_COUNT] = {0};
    static uint8_t interval_index = 0;
    static uint32_t signal_sum = 0;
    static uint32_t signal_count = 0;
    static uint32_t adaptive_threshold = HR_THRESHOLD_MIN;

    uint32_t current_time = get_time_ms();

    // 自适应阈值更新
    signal_sum += sensor_data.ac_ir;
    signal_count++;
    if (signal_count >= HR_ADAPTIVE_SAMPLE_COUNT) {
        adaptive_threshold = (uint32_t)((signal_sum / signal_count) * HR_THRESHOLD_RATIO);
        if (adaptive_threshold < HR_THRESHOLD_MIN) adaptive_threshold = HR_THRESHOLD_MIN;
        if (adaptive_threshold > HR_THRESHOLD_MAX) adaptive_threshold = HR_THRESHOLD_MAX;
        signal_sum = 0;
        signal_count = 0;
    }

    // 峰值检测
    if (sensor_data.ac_ir <= adaptive_threshold) return;

    if (sensor_data.ac_ir > last_peak_value) {
        last_peak_value = sensor_data.ac_ir;
        return;
    }

    if (last_peak_value == 0 || sensor_data.ac_ir >= last_peak_value * HR_PEAK_DROP_RATIO) return;

    // 检测到峰值下降
    uint32_t current_interval = current_time - last_peak_time;

    if (current_interval > HR_INTERVAL_MIN_MS && current_interval < HR_INTERVAL_MAX_MS) {
        peak_intervals[interval_index] = current_interval;
        interval_index = (interval_index + 1) % HR_PEAK_INTERVAL_COUNT;
        peak_count++;

        if (peak_count >= 2) {
            uint32_t avg_interval = 0;
            int valid_interval = 0;

            for (int i = 0; i < HR_PEAK_INTERVAL_COUNT; i++) {
                if (peak_intervals[i] > 0) {
                    avg_interval += peak_intervals[i];
                    valid_interval++;
                }
            }

            if (valid_interval > 0) {
                avg_interval = avg_interval / valid_interval;
                int hr = 60000 / avg_interval;

                if (hr >= HR_BPM_MIN && hr <= HR_BPM_MAX) {
                    // 异常值过滤：单次变化不超过20%
                    if (vital_signs.heart_rate_enable && vital_signs.heart_rate > 0) {
                        int hr_diff = hr - vital_signs.heart_rate;
                        if (hr_diff < 0) hr_diff = -hr_diff;
                        if (hr_diff > vital_signs.heart_rate * HR_CHANGE_LIMIT_RATIO) {
                            last_peak_value = 0;
                            last_peak_time = current_time;
                            return;
                        }
                    }

                    hr_samples[hr_sample_index] = hr;
                    hr_sample_index = (hr_sample_index + 1) % HR_WINDOW_SIZE;

                    // 加权平均（近期数据权重更高）
                    uint32_t hr_sum = 0;
                    uint32_t weight_sum = 0;
                    int valid_hr_samples = 0;
                    uint8_t newest_idx = (hr_sample_index + HR_WINDOW_SIZE - 1) % HR_WINDOW_SIZE;

                    for (int i = 0; i < HR_WINDOW_SIZE; i++) {
                        if (hr_samples[i] > 0) {
                            uint32_t age = (newest_idx + HR_WINDOW_SIZE - i) % HR_WINDOW_SIZE;
                            uint32_t weight = HR_WINDOW_SIZE - age;
                            hr_sum += hr_samples[i] * weight;
                            weight_sum += weight;
                            valid_hr_samples++;
                        }
                    }

                    if (valid_hr_samples > 0 && weight_sum > 0) {
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

// 血氧计算：基于红光/红外光比值(R值)的SpO2估算
static void calculate_spo2(void)
{
    if (sensor_data.dc_red == 0 || sensor_data.dc_ir == 0) return;

    float red_ratio = (float)sensor_data.ac_red / sensor_data.dc_red;
    float ir_ratio  = (float)sensor_data.ac_ir  / sensor_data.dc_ir;

    if (ir_ratio <= 0) return;

    float R = red_ratio / ir_ratio;
    float spo2_float = SPO2_OFFSET - SPO2_SLOPE * R;

    if (spo2_float < SPO2_MIN) spo2_float = SPO2_MIN;
    if (spo2_float > SPO2_MAX) spo2_float = SPO2_MAX;

    vital_signs.spo2 = (int)spo2_float;
    vital_signs.spo2_enable = 1;
}

// 主数据处理函数
bool main_max30102_data(void)
{
    uint32_t red, ir;

    max30102_read_fifo(&red, &ir);
    calculate_ac_components(red, ir);
    detect_heart_rate();
    calculate_spo2();

    if (vital_signs.heart_rate_enable && vital_signs.spo2_enable) {
        return_ac[0] = vital_signs.heart_rate;
        return_ac[1] = vital_signs.spo2;
        return true;
    }
    return false;
}

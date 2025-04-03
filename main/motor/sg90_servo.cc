#include "sg90_servo.h"
#include <esp_log.h>

static const char* TAG = "SG90Servo";

// LEDC 配置参数
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES       LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY      50    // SG90 舵机要求 50Hz
// 根据 14 位分辨率，约 0.5ms 脉宽对应的 duty 值
#define SERVO_MIN_DUTY      1638  
// 约 2.5ms 脉宽对应的 duty 值
#define SERVO_MAX_DUTY      8192  

SG90Servo::SG90Servo(gpio_num_t gpio) : gpio_(gpio), pwm_channel_(LEDC_CHANNEL_0) {
    // 配置 LEDC 定时器
    ledc_timer_config_t timer_config = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    // 配置 LEDC 通道
    ledc_channel_config_t channel_config = {
        .gpio_num       = gpio_,
        .speed_mode     = LEDC_MODE,
        .channel        = pwm_channel_,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = SERVO_MIN_DUTY,
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

SG90Servo::~SG90Servo() {
    // 停止 PWM 输出
    ledc_stop(LEDC_MODE, pwm_channel_, 0);
}

void SG90Servo::SetAngle(float angle) {
    // 检查并截断角度，同时打印警告信息
    if (angle < 0.0f) {
        ESP_LOGW(TAG, "Angle %.2f below 0, clamping to 0", angle);
        angle = 0.0f;
    }
    if (angle > 180.0f) {
        ESP_LOGW(TAG, "Angle %.2f above 180, clamping to 180", angle);
        angle = 180.0f;
    }
    
    // 线性映射角度到 duty 值
    uint32_t duty = SERVO_MIN_DUTY + (angle / 180.0f) * (SERVO_MAX_DUTY - SERVO_MIN_DUTY);
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, pwm_channel_, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, pwm_channel_));
}

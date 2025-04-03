#ifndef _SG90_SERVO_H_
#define _SG90_SERVO_H_

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <mutex>

class SG90Servo {
public:
    SG90Servo(gpio_num_t gpio);
    virtual ~SG90Servo();

    // 设置舵机角度（0~180 度）
    void SetAngle(float angle);

private:
    std::mutex mutex_;
    gpio_num_t gpio_;
    ledc_channel_t pwm_channel_;
};

#endif // _SG90_SERVO_H_

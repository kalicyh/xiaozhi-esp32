#include "servo_mcp_tool.h"
#include <esp_log.h>
#include <driver/ledc.h>

static const char* TAG = "ServoMcpTool";

// SG92R 舵机参数 (与 SG90 通用)
// SG92R 典型脉宽: 500µs (0°) - 2500µs (180°)
// 注意：使用 10-bit 分辨率时，占空比范围是 0-1023
#define SERVO_MIN_PULSEWIDTH_US 500   // 最小脉宽 (0°)
#define SERVO_MAX_PULSEWIDTH_US 2500  // 最大脉宽 (180°)
#define SERVO_MAX_ANGLE 180           // 最大角度
#define SERVO_FREQ 50                 // PWM 频率 (Hz)
#define SERVO_TIMER LEDC_TIMER_3      // 使用 TIMER_3 (TIMER_0=背光, TIMER_1=LED)
#define SERVO_CHANNEL LEDC_CHANNEL_5  // 使用 CHANNEL_5

ServoMcpTool::ServoMcpTool(gpio_num_t servo_pin)
    : servo_pin_(servo_pin), current_angle_(90.0f), initialized_(false) {
}

ServoMcpTool::~ServoMcpTool() {
    if (initialized_) {
        ledc_stop(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, 0);
    }
}

// 将角度转换为 LEDC 占空比
// 使用 14-bit 分辨率 (0-16383)
static uint32_t angle_to_duty(float angle) {
    // 计算脉宽 (us)
    float pulse_width_us = SERVO_MIN_PULSEWIDTH_US + 
        (angle / SERVO_MAX_ANGLE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US);
    
    // 周期 = 1000000us / 50Hz = 20000us
    // 占空比 = (脉宽 / 周期) * 16383 (14-bit 分辨率)
    uint32_t duty = (uint32_t)((pulse_width_us / 20000.0f) * 16383.0f);
    return duty;
}

void ServoMcpTool::InitServo() {
    ESP_LOGI(TAG, "Starting servo initialization on GPIO %d...", servo_pin_);
    
    // 配置 LEDC timer - 使用 14-bit 分辨率，适用于 50Hz
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,  // 14-bit 分辨率适合 50Hz
        .timer_num = SERVO_TIMER,
        .freq_hz = SERVO_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    
    ESP_LOGI(TAG, "Configuring LEDC timer %d with %dHz...", SERVO_TIMER, SERVO_FREQ);
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s (0x%x)", esp_err_to_name(ret), ret);
        return;
    }
    ESP_LOGI(TAG, "LEDC timer configured successfully");

    // 配置 LEDC channel
    ledc_channel_config_t channel_cfg = {
        .gpio_num = servo_pin_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = angle_to_duty(90.0f),  // 初始 90°
        .hpoint = 0,
        .flags = {
            .output_invert = 0,
        }
    };
    
    ESP_LOGI(TAG, "Configuring LEDC channel %d on GPIO %d...", SERVO_CHANNEL, servo_pin_);
    ret = ledc_channel_config(&channel_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s (0x%x)", esp_err_to_name(ret), ret);
        return;
    }
    ESP_LOGI(TAG, "LEDC channel configured successfully");

    initialized_ = true;
    current_angle_ = 90.0f;
    ESP_LOGI(TAG, "Servo initialized on GPIO %d (Timer %d, Channel %d)", 
             servo_pin_, SERVO_TIMER, SERVO_CHANNEL);
}

void ServoMcpTool::SetAngle(float angle) {
    if (!initialized_) {
        ESP_LOGW(TAG, "Servo not initialized");
        return;
    }

    // 限制角度范围
    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    uint32_t duty = angle_to_duty(angle);
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty: %s", esp_err_to_name(ret));
        return;
    }
    
    current_angle_ = angle;
    ESP_LOGI(TAG, "Servo angle set to: %.1f (duty: %lu)", angle, duty);
}

float ServoMcpTool::GetCurrentAngle() const {
    return current_angle_;
}

void ServoMcpTool::Initialize() {
    // 初始化舵机硬件
    InitServo();

    // 注册 MCP 工具
    auto& mcp_server = McpServer::GetInstance();

    // 设置舵机角度
    mcp_server.AddTool("self.servo.set_angle",
        "Set the servo motor angle (设置舵机角度).\n"
        "The angle range is 0 to 180 degrees.",
        PropertyList({
            Property("angle", kPropertyTypeInteger, 0, 180)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleSetAngle(properties);
        });

    // 获取当前角度
    mcp_server.AddTool("self.servo.get_angle",
        "Get the current servo motor angle (获取当前舵机角度).",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleGetAngle(properties);
        });

    ESP_LOGI(TAG, "ServoMcpTool initialized, current angle: %.1f", current_angle_);
}

ReturnValue ServoMcpTool::HandleSetAngle(const PropertyList& properties) {
    int angle = properties["angle"].value<int>();
    SetAngle(static_cast<float>(angle));
    return std::string("Servo angle set to " + std::to_string(angle) + " degrees");
}

ReturnValue ServoMcpTool::HandleGetAngle(const PropertyList& properties) {
    return static_cast<int>(current_angle_);
}

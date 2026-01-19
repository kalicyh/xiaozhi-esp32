#ifndef SERVO_MCP_TOOL_H
#define SERVO_MCP_TOOL_H

#include "mcp_server.h"
#include <driver/gpio.h>

// SG92R 舵机 MCP 控制工具类
class ServoMcpTool {
private:
    gpio_num_t servo_pin_;
    float current_angle_;
    bool initialized_;

public:
    ServoMcpTool(gpio_num_t servo_pin);
    ~ServoMcpTool();
    
    // 初始化舵机和注册 MCP 工具
    void Initialize();
    
    // 获取当前角度
    float GetCurrentAngle() const;

private:
    // 初始化舵机硬件
    void InitServo();
    
    // 设置舵机角度 (0-180)
    void SetAngle(float angle);
    
    // MCP 工具回调
    ReturnValue HandleSetAngle(const PropertyList& properties);
    ReturnValue HandleGetAngle(const PropertyList& properties);
};

#endif // SERVO_MCP_TOOL_H

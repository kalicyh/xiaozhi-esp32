#include "dual_network_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "adc_battery_monitor.h"
#include "led/single_led.h"
#include "wifi_board.h"
#include "assets/lang_config.h"
#include "device_state.h"
#include <string>

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#include "driver/gpio.h"

#include "display/display.h"

#define TAG "KalicyhS3ML307"

class KalicyhS3ML307 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    PowerSaveTimer* power_save_timer_;
    AdcBatteryMonitor* adc_battery_monitor_ = nullptr;
    NoDisplay no_display_;

    void InitializePowerManager() {
        adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_7, 100000, 100000, GPIO_NUM_46);
        adc_battery_monitor_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 600);
        power_save_timer_->SetEnabled(true);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI) {
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    // cast to WifiBoard
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.EnterWifiConfigMode();
                }
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            ESP_LOGI(TAG, "Button OnDoubleClick");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring || app.GetDeviceState() == kDeviceStateIdle) {
                SwitchNetworkType();
            }
        });
    }

public:
    KalicyhS3ML307() : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, GPIO_NUM_NC),
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitializePowerManager();
        InitializeCodecI2c();
        InitializeButtons();
    }

    virtual Led* GetLed() override {
        static SingleLed led_strip(BUILTIN_LED_GPIO);
        return &led_strip;
    }

    virtual Display* GetDisplay() override {
        return &no_display_;
    }

    virtual Backlight* GetBacklight() override {
        return nullptr;  // 不使用背光
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = adc_battery_monitor_->IsCharging();
        discharging = adc_battery_monitor_->IsDischarging();
        level = adc_battery_monitor_->GetBatteryLevel();
        return true;
    }

    // 重写 DualNetworkBoard 的方法，代理到当前活动的板子
    virtual void StartNetwork() override {
        DualNetworkBoard::StartNetwork();
    }

    virtual NetworkInterface* GetNetwork() override {
        return DualNetworkBoard::GetNetwork();
    }

    virtual const char* GetNetworkStateIcon() override {
        return DualNetworkBoard::GetNetworkStateIcon();
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveLevel(level);
    }

    virtual std::string GetBoardJson() override {
        return DualNetworkBoard::GetBoardJson();
    }

    virtual std::string GetDeviceStatusJson() override {
        return DualNetworkBoard::GetDeviceStatusJson();
    }
};

DECLARE_BOARD(KalicyhS3ML307);

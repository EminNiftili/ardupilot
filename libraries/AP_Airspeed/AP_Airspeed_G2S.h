#pragma once

#include "AP_Airspeed_Backend.h"
#include <AP_Param/AP_Param.h>
#include <AP_CANManager/AP_CANSensor.h>
#include <AP_HAL/AP_HAL.h>


class AP_Airspeed_G2S : public AP_Airspeed_Backend {
public:
    AP_Airspeed_G2S(AP_Airspeed &ap, uint8_t inst);

    bool init(void) override;

    bool get_temperature(float &temperature) override;

    bool has_airspeed() override { return true; }

    bool get_airspeed(float &airspeed) override;

    int8_t enabled() const { return _ap.g2s_enabled(_inst); }
    float offset() const { return _ap.g2s_offset(_inst); }
    int8_t failsafe_en() const { return _ap.g2s_failsafe(_inst); }



private:
    MultiCAN *_multican = nullptr;

    uint32_t _last_update_ms = 0;

    uint16_t _recv_accept_id = 0; // 0 => for every İD

    bool handle_frame(AP_HAL::CANFrame &frame);

    AP_Airspeed &_ap;
    const uint8_t _inst;

    float _last_airspeed_ms = 0.0f;
    float _last_temp_c = 0.0f;
    bool  _healthy = false;

    uint32_t _last_publish_ms = 0;
    float    _last_published_airspeed_mps = 0.0f;

    static constexpr uint32_t DATA_TIMEOUT_MS = 200; // 0.2s = 50Hz

    void update_health();

    bool _failsafe_active = false;
    uint32_t _last_failsafe_msg_ms = 0;
};

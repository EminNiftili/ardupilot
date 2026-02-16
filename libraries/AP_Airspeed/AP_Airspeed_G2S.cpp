#include "AP_Airspeed_G2S.h"
#include "AP_Airspeed.h"
#include <GCS_MAVLink/GCS.h>


AP_Airspeed_G2S::AP_Airspeed_G2S(AP_Airspeed &ap, uint8_t inst)
    : AP_Airspeed_Backend(ap, inst)
    , _ap(ap)
    , _inst(inst)
{
    _last_airspeed_ms = 0.0f;
    _last_temp_c = 0.0f;
    _healthy = false;

    
    _multican = NEW_NOTHROW MultiCAN{
        FUNCTOR_BIND_MEMBER(&AP_Airspeed_G2S::handle_frame, bool, AP_HAL::CANFrame &),
        AP_CAN::Protocol::Scripting2,
        "G2S Airspeed"
    };
    if (_multican == nullptr) {
        AP_HAL::panic("Failed to create G2S MultiCAN");
    }
}

bool AP_Airspeed_G2S::init(void)
{
    if (enabled() == 0) {
        _healthy = false;
        return false;
    }

    _healthy = false;
    return true;
}

bool AP_Airspeed_G2S::get_temperature(float &temperature)
{
    update_health();
    if (_failsafe_active) {
        return false;
    }
    if (!_healthy) {
        return false;
    }

    temperature = _last_temp_c;
    return true;
}


bool AP_Airspeed_G2S::get_airspeed(float &airspeed)
{
    update_health();
    if (_failsafe_active) {
        return false;
    }
    if (!_healthy) {
        return false;
    }

    const uint32_t now = AP_HAL::millis();

    if (now - _last_publish_ms < 20) {
        airspeed = _last_published_airspeed_mps;
        return true;
    }

    _last_publish_ms = now;

    _last_published_airspeed_mps = _last_airspeed_ms + offset();
    airspeed = _last_published_airspeed_mps;

    return true;
}


bool AP_Airspeed_G2S::handle_frame(AP_HAL::CANFrame &frame)
{
    const uint16_t id = frame.id;

    if (_recv_accept_id != 0 && id != _recv_accept_id) {
        return false;
    }
    
    const uint8_t len = AP_HAL::CANFrame::dlcToDataLength(frame.dlc);
    if (len < 4) {
        return false;
    }

    float airspeed_ms = 0.0f;
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    memcpy(&airspeed_ms, frame.data, 4);

    if (!isfinite(airspeed_ms) || airspeed_ms < -10.0f || airspeed_ms > 200.0f) {
        return false;
    }

    _last_airspeed_ms = airspeed_ms;
    _last_update_ms = AP_HAL::millis();
    _healthy = true;

    return true;
}

void AP_Airspeed_G2S::update_health()
{
    const uint32_t now = AP_HAL::millis();

    if (enabled() == 0) {
        _healthy = false;
        _failsafe_active = false;
        return;
    }

    if (_last_update_ms == 0) {
        _healthy = false;
    } else if (now - _last_update_ms > DATA_TIMEOUT_MS) {
        _healthy = false;
    } else {
        _healthy = true;
    }

    if (!_healthy && failsafe_en() != 0) {
        _failsafe_active = true;

        if (now - _last_failsafe_msg_ms > 5000) {
            _last_failsafe_msg_ms = now;
            GCS_SEND_TEXT(MAV_SEVERITY_WARNING, "G2S Airspeed FAILSAFE: no CAN data");
        }
    } else {
        _failsafe_active = false;
    }
}



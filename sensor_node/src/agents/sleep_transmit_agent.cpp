#include "agents/sleep_transmit_agent.h"

RTC_DATA_ATTR bool SleepTransmitAgent::already_initialized = false;
RTC_DATA_ATTR float SleepTransmitAgent::deep_sleep_duration = 0;

SleepTransmitAgent::SleepTransmitAgent(
    GoalOrientedTensor *got,
    float got_weight,
    float energy_weight,
    uint8_t threshold,
    bool consider_delay,
    bool state_aware
) {
    this->got = got;
    
    // don't continue initializing if data from previous initialization exist
    if (SleepTransmitAgent::already_initialized) {
        return;
    }
    
    this->state_aware = state_aware;
    
    // TODO This is precalculated for 1.01% drop rate and does not consider earlier sleeping
    float avg_antenna_power_interval = 104;
    float antenna_energy = avg_antenna_power_interval * ANTENNA_MJ_PER_MS;

    if (threshold == UINT8_MAX) {
        float energy = energy_weight * (SENSING_ENERGY_MJ + WAKE_UP_ENERGY_MJ + antenna_energy);
        threshold = static_cast<int>(sqrt(2.0 * energy / got_weight)) + 1;
    } 
    this->base_threshold = threshold;

    if (consider_delay) {
        threshold += 0; // min(env.correlated_channel.forward_delays)
    }

    this->deep_sleep_duration = threshold;

    SleepTransmitAgent::already_initialized = true;
}

bool SleepTransmitAgent::antenna_action(const EnvState env_state[NUM_DIMS]) {
    if (this->got->get_metric() != "aos" || !this->state_aware) {
        return true;
    }

    for (unsigned dim = 0; dim < NUM_DIMS; dim++) {
        if (env_state[dim].X_proc != env_state[dim].X_rx) {
            return true;
        }
    }
    
    return false;
}

uint8_t SleepTransmitAgent::sleep_action(const EnvState env_state[NUM_DIMS]) {
    return this->deep_sleep_duration;
}

void SleepTransmitAgent::update_after_sensing_step(const EnvState env_state[NUM_DIMS]) {}

void SleepTransmitAgent::update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) {}

void SleepTransmitAgent::update_after_sleep_step(const EnvState env_state[NUM_DIMS]) {}

void SleepTransmitAgent::reset(GoalOrientedTensor *got) {}


void SleepTransmitAgent::save_to_nvs(const nvs_handle_t nvs_handle) {}

void SleepTransmitAgent::load_from_nvs(const nvs_handle_t nvs_handle) {}
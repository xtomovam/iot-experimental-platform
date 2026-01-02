#pragma once

#include "common.h"
#include "env/got.h"
#include "aos_table.h"
#include "half.hpp"

class Agent {
public:
    virtual ~Agent() = default;
    
    virtual bool antenna_action(const EnvState env_state[NUM_DIMS]);
    virtual uint8_t sleep_action(const EnvState env_state[NUM_DIMS]);
    virtual void update_after_sensing_step(const EnvState env_state[NUM_DIMS]);
    virtual void update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms);
    virtual void update_after_sleep_step(const EnvState env_state[NUM_DIMS]);
    virtual void reset(GoalOrientedTensor *got);

    virtual void save_to_nvs(const nvs_handle_t nvs_handle);
    virtual void load_from_nvs(const nvs_handle_t nvs_handle);

private:
    bool omniscient = false;
};
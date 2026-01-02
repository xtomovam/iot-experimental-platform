#include "agents/random_agent.h"

RandomAgent::RandomAgent() {}

bool RandomAgent::antenna_action(const EnvState env_state[NUM_DIMS]) {
    return custom_random(0, 1) < 0.5;
}

uint8_t RandomAgent::sleep_action(const EnvState env_state[NUM_DIMS]) {
    return custom_random_uint8(0, 10) * 30;
}

void RandomAgent::update_after_sensing_step(const EnvState env_state[NUM_DIMS]) {}

void RandomAgent::update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) {}

void RandomAgent::update_after_sleep_step(const EnvState env_state[NUM_DIMS]) {}

void RandomAgent::reset(GoalOrientedTensor *got) {}


void RandomAgent::save_to_nvs(const nvs_handle_t nvs_handle) {}

void RandomAgent::load_from_nvs(const nvs_handle_t nvs_handle) {}
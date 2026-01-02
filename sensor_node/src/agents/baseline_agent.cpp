#include "agents/baseline_agent.h"

BaselineAgent::BaselineAgent(const BaselineAgentType type) : type(type) {}

bool BaselineAgent::antenna_action(const EnvState env_state[NUM_DIMS]) {
    return type == BaselineAgentType::ALWAYS_SEND;
}

uint8_t BaselineAgent::sleep_action(const EnvState env_state[NUM_DIMS]) {
    return type == BaselineAgentType::ALWAYS_SEND ? 0 : UINT8_MAX; 
}

void BaselineAgent::update_after_sensing_step(const EnvState env_state[NUM_DIMS]) {}

void BaselineAgent::update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) {}

void BaselineAgent::update_after_sleep_step(const EnvState env_state[NUM_DIMS]) {}

void BaselineAgent::reset(GoalOrientedTensor *got) {}


void BaselineAgent::save_to_nvs(const nvs_handle_t nvs_handle) {}

void BaselineAgent::load_from_nvs(const nvs_handle_t nvs_handle) {}
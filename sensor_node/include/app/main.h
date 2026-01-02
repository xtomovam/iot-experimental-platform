#pragma once

#include "common.h"

#include "app/ble.h"
#include "app/feedback.h"
#include "app/sense.h"

// agents
#include "agents/baseline_agent.h"
#include "agents/random_agent.h"
#include "agents/sleep_transmit_agent.h"
#include "agents/q_learning_agent.h"
#include "agents/psbo_agent.h"

#include "env/got.h" // GoalOrientedTensor
#include "nvs_helper.h" // save_agent, load_agent

extern uint32_t seq;
extern uint32_t curr_ts;
extern uint32_t ts_sensed;
extern GoalOrientedTensor *got;
extern Agent *agent;
extern EnvState env_state[NUM_DIMS];
extern uint8_t AoS;
extern bool antenna_action;
extern float energy_consumed_mJ;
extern uint64_t transmission_start;
extern time_t max_saving_time;

void timestep_end();
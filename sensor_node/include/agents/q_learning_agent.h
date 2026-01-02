#pragma once

#include "agent.h"

struct QLearningAgentData {
    float got_weight;
    float energy_weight;
    float learning_rate;
    float discount_factor;
    float initial_epsilon;
    float epsilon;
    float decay;
    float accumulated_cost;

    uint8_t sleep_action_space[MAX_AOI];

    uint8_t realistic_states;
    uint8_t simplified;
    uint8_t omniscient;

    EnvState state_before_sleep[NUM_DIMS];
    uint8_t last_sleep_action;
};

class QLearningAgent : public Agent {
public:
    QLearningAgent(
        GoalOrientedTensor *got,
        float got_weight,
        float energy_weight,
        uint32_t total_steps,
        float learning_rate = 0.05,
        float discount_factor = 0.9,
        float initial_epsilon = 0.1,
        float final_epsilon = 0.01,
        bool realistic_states = false,
        bool simplified = false
    );
    
    bool antenna_action(const EnvState env_state[NUM_DIMS]) override;
    uint8_t sleep_action(const EnvState env_state[NUM_DIMS]) override;
    void update_after_sensing_step(const EnvState env_state[NUM_DIMS]) override;
    void update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) override;
    void update_after_sleep_step(const EnvState env_state[NUM_DIMS]) override;
    void reset(GoalOrientedTensor *got) override;

    void save_to_nvs(const nvs_handle_t nvs_handle);
    void load_from_nvs(const nvs_handle_t nvs_handle);
    
    void omniscient_update(const EnvState next_state[NUM_DIMS], uint8_t sleep_action, float cost);
    bool is_omniscient();
    
private:
    static bool already_initialized;
    GoalOrientedTensor *got;
    QLearningAgentData data;
    half_float::half q_table[NUM_DIMS][NUM_STATE_COMBINATIONS][NUM_AOI_BUCKETS] = {{{half_float::half(0)}}};

    inline float& got_weight() { return data.got_weight; }
    inline float& energy_weight() { return data.energy_weight; }
    inline float& learning_rate() { return data.learning_rate; }
    inline float& discount_factor() { return data.discount_factor; }
    inline float& initial_epsilon() { return data.initial_epsilon; }
    inline float& epsilon() { return data.epsilon; }
    inline float& decay() { return data.decay; }
    inline uint8_t& sleep_action_space(size_t index) { return data.sleep_action_space[index]; }
    inline uint8_t& realistic_states() { return data.realistic_states; }
    inline uint8_t& simplified() { return data.simplified; }
    inline uint8_t& omniscient() { return data.omniscient; }
    inline EnvState& state_before_sleep(size_t dim) { return data.state_before_sleep[dim]; }
    inline uint8_t& last_sleep_action() { return data.last_sleep_action; }
    inline float& accumulated_cost() { return data.accumulated_cost; }
};
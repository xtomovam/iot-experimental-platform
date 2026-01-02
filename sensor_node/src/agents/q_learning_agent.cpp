#include "agents/q_learning_agent.h"

#include <cfloat>

RTC_DATA_ATTR bool QLearningAgent::already_initialized = false;

// Q-table functions

uint16_t encode_state(const EnvState env_state[NUM_DIMS]) { // single dimension encoded
    // extract relevant fields
    uint8_t X_tx = env_state[0].X_tx;
    uint8_t X_rx   = env_state[0].X_rx;
    uint8_t AoI_rx = env_state[0].AoI_rx;
    uint8_t AoII   = env_state[0].AoII;
    
    // find AoI_rx and AoII buckets
    uint8_t AoI_rx_bucket = NUM_AOI_BUCKETS - 1;
    uint8_t AoII_bucket = NUM_AOI_BUCKETS - 1;
    for (size_t i = 0; i < NUM_AOI_BUCKETS; i++) {
        if (AoI_rx <= AOI_BUCKETS_MAX[i]) {
            AoI_rx_bucket = i;
            break;
        }
    } for (size_t i = 0; i < NUM_AOI_BUCKETS; i++) {
        if (AoII <= AOI_BUCKETS_MAX[i]) {
            AoII_bucket = i;
            break;
        }
    }

    // compute pair index for (AoI_rx_bucket, AoII_bucket)
    uint8_t pair_index = (AoI_rx_bucket * (AoI_rx_bucket + 1)) / 2 + AoII_bucket;

    // encode state
    uint16_t code = ((X_tx * 7) + X_rx) * 10 + pair_index;

    return code;
}

size_t find_min_qvalue_index(const half_float::half q_table[NUM_STATE_COMBINATIONS][NUM_AOI_BUCKETS], const EnvState &state) {
    half_float::half min = half_float::half(65504.0f); // maximum value for half_float::half
    size_t min_index = SIZE_MAX;
    uint16_t encoded_state = encode_state(&state);
    for (size_t i = 0; i < NUM_AOI_BUCKETS; i++) {
        if (q_table[encoded_state][i] < min) {
            min = q_table[encoded_state][i];
            min_index = i;
        }
    }

    if (min_index == SIZE_MAX) {
        throw std::runtime_error("No minimum q_value found in Q-table for the given state (NUM_AOI_BUCKETS = 0).");
    }
    return min_index;
}

// Q-learning agent functions

QLearningAgent::QLearningAgent(
    GoalOrientedTensor *got,
    float got_weight,
    float energy_weight,
    uint32_t total_steps,
    float learning_rate,
    float discount_factor,
    float initial_epsilon,
    float final_epsilon,
    bool realistic_states,
    bool simplified
) {
    this->got = got;

    if (QLearningAgent::already_initialized) {
        return;
    }

    this->got_weight() = got_weight;
    this->energy_weight() = energy_weight;

    this->learning_rate() = learning_rate;
    this->discount_factor() = discount_factor;
    this->initial_epsilon() = initial_epsilon;
    this->epsilon() = initial_epsilon;
    this->decay() = pow(final_epsilon, (1.0 / total_steps));

    // set all values in q_table to 0
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        for (size_t i = 0; i < NUM_STATE_COMBINATIONS; i++) {
            for (size_t j = 0; j < NUM_AOI_BUCKETS; j++) {
                this->q_table[dim][i][j] = 0;
            }
        }
    }

    for (uint8_t i = 0; i < MAX_AOI; ++i) {
        this->sleep_action_space(i) = i;
    }

    this->realistic_states() = realistic_states;
    this->simplified() = simplified;

    this->omniscient() = true;

    // no prior state
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->state_before_sleep(dim).X_proc = INT16_MAX;
        this->state_before_sleep(dim).X_tx = INT16_MAX;
        this->state_before_sleep(dim).X_rx = INT16_MAX;
        this->state_before_sleep(dim).AoI_rx = UINT8_MAX;
        this->state_before_sleep(dim).AoII = UINT8_MAX;
    }

    this->accumulated_cost() = 0.0;

    this->last_sleep_action() = UINT8_MAX;

    // assertion to ensure realistic_states and simplified are not both true
    if (this->realistic_states() && this->simplified()) {
        throw std::runtime_error("QLearningAgent cannot have both realistic_states and simplified set to true.");
    }

    QLearningAgent::already_initialized = true;
}

bool QLearningAgent::antenna_action(const EnvState env_state[NUM_DIMS]) {
    if (this->got->get_metric() ==  "aoi") {
        return true;
    } else  {
        for (unsigned dim = 0; dim < NUM_DIMS; dim++) {
            if (env_state[dim].X_proc != env_state[dim].X_rx) {
                return true;
            }
        }
    }
    return false;
}

uint8_t QLearningAgent::sleep_action(const EnvState env_state[NUM_DIMS]) {
    uint8_t planned_sleep_duration;
    float rand_val = custom_random(0.0, 1.0);

    // ε-Greedy: With probability epsilon, we choose a random action (Exploration)
    if (rand_val < this->epsilon()) {
        planned_sleep_duration = this->sleep_action_space((size_t)custom_random(0.0f, (float)MAX_AOI));
    } else {
        // otherwise, we choose the action with the lowest Q-value (Exploitation)
        size_t aoi_bucket = find_min_qvalue_index(this->q_table[0], env_state[0]); // single dimension
        planned_sleep_duration = random_from_aoi_bucket(aoi_bucket);
    }
    return planned_sleep_duration;
}

void QLearningAgent::update_after_sensing_step(const EnvState env_state[NUM_DIMS]) {}

void QLearningAgent::update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) {}

void QLearningAgent::update_after_sleep_step(const EnvState env_state[NUM_DIMS]) {}

bool QLearningAgent::is_omniscient() {
    return this->omniscient();
}

void QLearningAgent::omniscient_update(const EnvState next_state[NUM_DIMS], uint8_t sleep_action, float cost) { // single dimension hard-coded
    if (sleep_action != UINT8_MAX) {
        if (this->state_before_sleep(0).X_proc == INT16_MAX) {

            for (size_t dim = 0; dim < NUM_DIMS; dim++) {
                this->state_before_sleep(dim) = next_state[dim];
            }

            this->last_sleep_action() = sleep_action;

        } else {

            uint8_t best_next_action_bucket = find_min_qvalue_index(this->q_table[0], next_state[0]);
            uint8_t best_next_action = random_from_aoi_bucket(best_next_action_bucket); // random within the bucket

            half_float::half v = this->q_table[0][encode_state(&next_state[0])][best_next_action_bucket];

            this->q_table[0][encode_state(&this->state_before_sleep(0))][this->last_sleep_action()] += this->learning_rate() * (this->accumulated_cost() + this->discount_factor() * v - this->q_table[0][encode_state(&this->state_before_sleep(0))][this->last_sleep_action()]); 

            this->epsilon() *= this->decay();

            this->accumulated_cost() = 0.0;
            this->last_sleep_action() = sleep_action;
            for (size_t dim = 0; dim < NUM_DIMS; dim++) {
                this->state_before_sleep(dim) = next_state[dim];
            }
        }
    }

    this->accumulated_cost() += cost / (this->last_sleep_action() + 1);
}

void QLearningAgent::reset(GoalOrientedTensor *got) {
    this->epsilon() = this->initial_epsilon();
    this->accumulated_cost() = 0.0;
    this->last_sleep_action() = UINT8_MAX;
    
    // empty the Q-table
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        for (size_t i = 0; i < NUM_STATE_COMBINATIONS; i++) {
            for (size_t j = 0; j < NUM_AOI_BUCKETS; j++) {
                this->q_table[dim][i][j] = 0;
            }
        }
    }

    // reset state_before_sleep
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->state_before_sleep(dim).X_proc = INT16_MAX;
        this->state_before_sleep(dim).X_tx = INT16_MAX;
        this->state_before_sleep(dim).X_rx = INT16_MAX;
        this->state_before_sleep(dim).AoI_rx = UINT8_MAX;
        this->state_before_sleep(dim).AoII = UINT8_MAX;
    }
}

void QLearningAgent::save_to_nvs(const nvs_handle_t nvs_handle) {
    // save main struct
    size_t agent_data_size = sizeof(QLearningAgentData);
    esp_err_t err = nvs_set_blob(nvs_handle, "q_agent_data", &this->data, agent_data_size);
    if (err != ESP_OK) {
        throw std::runtime_error("Failed to save q_learning_agent_data to NVS.");
    }

    // save Q-table
    size_t q_table_size = sizeof(this->q_table);
    err = nvs_set_blob(nvs_handle, "q_table", this->q_table, q_table_size);
    if (err != ESP_OK) {
        throw std::runtime_error("Failed to save q_table to NVS.");
    }

    // commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        throw std::runtime_error("Failed to commit NVS after saving QLearningAgent.");
    }

    // close NVS handle
    nvs_close(nvs_handle);
}

void QLearningAgent::load_from_nvs(const nvs_handle_t nvs_handle) {
    // load main struct
    esp_err_t err;
    size_t required_size;
    required_size = sizeof(this->data);
    err = nvs_get_blob(nvs_handle, "q_agent_data", &this->data, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        throw std::runtime_error("Failed to load q_learning_agent_data from NVS.");
    }

    // load Q-table
    required_size = sizeof(this->q_table);
    err = nvs_get_blob(nvs_handle, "q_table", this->q_table, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        throw std::runtime_error("Failed to load q_table from NVS.");
    }

    // close NVS handle
    nvs_close(nvs_handle);
}
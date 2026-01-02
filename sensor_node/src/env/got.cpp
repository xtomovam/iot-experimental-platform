#include "env/got.h"

// compute expected AOI cost using sender belief and receiver conditional beliefs
double fast_aoi_cost(
    const CostFunction &cost_tensor,
    const double belief_process_state_at_receiver[NUM_STATES][NUM_STATES],
    const double belief_process_state[NUM_STATES],
    const uint32_t aoi_receiver
) {
    double additional_cost = 0.0;
    // iterate over true process states (sender belief)
    for (int X_proc = 0; X_proc < NUM_STATES; X_proc++) {
        double prob_ps = belief_process_state[X_proc];
        if (fabsf(prob_ps) < EPSILON) continue; // skip zero-probability process states

        // copy receiver's conditional belief for this process state to a local array
        double b_rec[NUM_STATES];
        for (size_t i = 0; i < NUM_STATES; i++) {
            b_rec[i] = belief_process_state_at_receiver[X_proc][i];
        }

        // iterate over possible receiver states and accumulate weighted cost
        for (int X_rx = 0; X_rx < NUM_STATES; X_rx++) {
            double prob_rs = b_rec[X_rx];
            if (fabsf(prob_rs) < EPSILON) continue; // skip zero-probability receiver states

            uint8_t aos = aoi_receiver < MAX_AOI ? aoi_receiver : MAX_AOI;
            additional_cost += ((double)prob_rs * (double)prob_ps * cost_tensor(X_proc, X_rx, aos));
        }
    }
    return (double)additional_cost;
}

// compute expected MSE cost using sender belief and receiver conditional beliefs
double fast_mse_cost(
    const CostFunction &cost_tensor, 
    const double belief_process_state_at_receiver[NUM_STATES][NUM_STATES], 
    const double belief_process_state[NUM_STATES]
) {
    return fast_aoi_cost(
        cost_tensor,
        belief_process_state_at_receiver,
        belief_process_state,
        0 // AoI is not used in MSE cost
    );
}

// compute expected AoS cost using sparse representation of AoS probabilities
double fast_aos_cost_sparse(
    const CostFunction &cost_tensor,
    const double belief_process_state_at_receiver[NUM_STATES][NUM_STATES],
    const double belief_process_state[NUM_STATES],
    AoSTable &belief_aos_table
) {
    double additional_cost = 0.0;

    // iterate over non-zero AoS entries in the sparse table (O(k) where k is number of non-zero entries)
    for (auto &pair : belief_aos_table.get_table()) {
        const Key key = pair.first;
        double p_aos = pair.second;
        if (fabsf(p_aos) < EPSILON) {
            continue; // skip zero-probability AoS entries
        }

        // probability of the true process state (sender belief)
        double p_ps = belief_process_state[key.X_proc];
        if (fabsf(p_ps) < EPSILON) {
            continue; // skip zero-probability process states
        }

        // conditional probability of receiver state given process state
        double p_rs = belief_process_state_at_receiver[key.X_proc][key.X_rx];
        if (fabsf(p_rs) < EPSILON) {
            continue; // skip zero-probability receiver states
        }

        // accumulate weighted cost (AoS entry multiplies the joint probability)
        uint8_t aos = key.AoII < MAX_AOI ? key.AoII : MAX_AOI;
        additional_cost += (double)p_ps * (double)p_rs * (double)p_aos * cost_tensor(key.X_proc, key.X_rx, aos);
    }
    
    return (double)additional_cost;
}

// returns the index of given state in given array (O(n) where n is number of possible states)
size_t state_to_index(
    const int16_t states[NUM_STATES],
    const int16_t state
) {
    for (size_t i = 0; i < NUM_STATES; i++) {
        if (states[i] == state) {
            return i;
        }
    }
    throw std::runtime_error("State not found in states array");
}

// GoT constructor
GoalOrientedTensor::GoalOrientedTensor(
    const char metric[],
    const int16_t states[NUM_DIMS][NUM_STATES],
    const double base_values[NUM_DIMS][NUM_STATES][NUM_STATES],
    const double growths[NUM_DIMS][NUM_STATES][NUM_STATES],
    const double mse_scaling
) {
    strncpy(this->metric, metric, MAX_METRIC_NAME_LEN);
    for (unsigned dim = 0; dim < NUM_DIMS; dim++) {
        for (unsigned state = 0; state < NUM_STATES; state++) {
            this->states[dim][state] = states[dim][state];
            for (unsigned jstate = 0; jstate < NUM_STATES; jstate++) {
                this->base_values[dim][state][jstate] = base_values[dim][state][jstate];
                this->growths[dim][state][jstate] = growths[dim][state][jstate];
            }
        }
    }
    this->mse_scaling = mse_scaling;

    this->generate_tensor();
};

// create cost function based on specified metric
CostFunction GoalOrientedTensor::make_cost_function(const size_t dim) {
    if (strcmp(this->metric, "aoi") == 0) {
        return [](int16_t X_tx, int16_t X_rx, uint8_t AoI_rx) {
            return (double)AoI_rx;
        };
    }
    if (strcmp(this->metric, "aos") == 0) {
        return [](int16_t X_tx, int16_t X_rx, uint8_t AoI_rx) {
            return (double)AoI_rx;
        };
    }
    if (strcmp(this->metric, "mse") == 0) {
        this->mse_scaling = this->mse_scaling == -1 ? 1 : this->mse_scaling;
        return [this, dim](int16_t X_tx, int16_t X_rx, uint8_t AoI_rx) {
            return this->mse_scaling * pow(this->states[dim][X_tx] - this->states[dim][X_rx], 2);
        };
    }
    if (strcmp(this->metric, "general") == 0) { // generate random cost function
        this->mse_scaling = this->mse_scaling == -1 ? 1000 : this->mse_scaling;

        return [this, dim](int X_tx, int X_rx, int AoI_rx) {
            double cost = this->base_values[dim][X_tx][X_rx] + this->growths[dim][X_tx][X_rx] * AoI_rx;
            return cost;
        };
    }
    if (strcmp(this->metric, "specific") == 0) {
        // will not be used in esperiments
        throw std::runtime_error("Specific metric not implemented");
        exit(EXIT_FAILURE);
    }
    throw std::runtime_error("Unknown metric type");
    exit(EXIT_FAILURE);
}

// generate cost tensor for each dimension
void GoalOrientedTensor::generate_tensor() {
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->cost_tensor[dim] = make_cost_function(dim);
    }

    if (strcmp(this->metric, "general") == 0) {
        strncpy(this->metric, "aos", MAX_METRIC_NAME_LEN);
    } else if (strcmp(this->metric, "specific") == 0) {
        strncpy(this->metric, "aos", MAX_METRIC_NAME_LEN);
    }

}

// compute GoT cost
double GoalOrientedTensor::get_cost(
    const int16_t process_state[NUM_DIMS],
    const int16_t receiver_state[NUM_DIMS],
    const uint8_t aoi_receiver,
    const uint8_t aos
    ) const {
    double cost = 0;

    // for each dimension
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        // get indices of process and receiver states
        size_t p_state_ind = state_to_index(this->states[dim],
            process_state[dim]
        );
        size_t r_state_ind = state_to_index(
            this->states[dim],
            receiver_state[dim]
        );

        // determine index for cost function based on metric
        size_t index;
        if (strcmp(this->metric, "aoi") == 0) {
            index = aoi_receiver < MAX_AOI ? aoi_receiver : MAX_AOI;
        } else if (strcmp(this->metric, "aos") == 0) {
            index = aos < MAX_AOI ? aos : MAX_AOI;
        } else if (strcmp(this->metric, "mse") == 0) {
            index = 0;
        } else {
            throw std::runtime_error("Unknown metric type in get_cost");
            exit(EXIT_FAILURE);
        }

        // compute and accumulate normalized cost
        double dim_cost = this->cost_tensor[dim](p_state_ind, r_state_ind, index) / NUM_DIMS;

        cost += dim_cost;
    }

    return cost;
}

// compute GoT belief cost
double GoalOrientedTensor::get_belief_cost(
    const double belief_process_state[NUM_DIMS][NUM_STATES],
    const double belief_process_state_at_receiver[NUM_DIMS][NUM_STATES][NUM_STATES],
    const uint32_t aoi_receiver,
    AoSTable belief_aos_table[NUM_DIMS]
) const {
    double cost = 0;
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        if (strcmp(this->metric, "aoi") == 0) {
            cost += fast_aoi_cost(
                this->cost_tensor[dim],
                belief_process_state_at_receiver[dim],
                belief_process_state[dim],
                aoi_receiver
            );
        } else if (strcmp(this->metric, "aos") == 0) {
            cost += fast_aos_cost_sparse(
                this->cost_tensor[dim],
                belief_process_state_at_receiver[dim],
                belief_process_state[dim],
                belief_aos_table[dim]
            );
        } else if (strcmp(this->metric, "mse") == 0) {
            cost += fast_mse_cost(
                this->cost_tensor[dim],
                belief_process_state_at_receiver[dim],
                belief_process_state[dim]
            );
        } else {
            throw std::runtime_error("Unknown metric type in get_belief_cost");
        }
    }
    return cost / NUM_DIMS;
}


// getters and setters

const char *GoalOrientedTensor::get_metric() const {
    return this->metric;
}

StatesType *GoalOrientedTensor::get_states() {
    return &this->states;
}

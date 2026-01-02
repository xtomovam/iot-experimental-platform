#include "agents/psbo_agent.h"

// cached powered matrices for backwards probability calculation
static double cached_powered[MAX_AOI + 1][NUM_STATES][NUM_STATES];
static bool cached_powered_valid[MAX_AOI + 1] = {false};

// helper function to update AoS table
AoSTable static_update_belief_aos(
    const AoSTable &old_table,
    const double belief[NUM_STATES],
    const double prev_belief[NUM_STATES],
    const double P[NUM_STATES][NUM_STATES],
    uint8_t max_aos
) {
    AoSTable new_table;
    AoSTable rolled_rows;

    // roll AoS rows
    for (const auto &pair : old_table.get_table()) {
        const Key &old_key = pair.first;
        double val = pair.second;

        if (val < EPSILON) continue;

        uint8_t new_aos = old_key.AoII + 1;
        if (new_aos > max_aos) continue;

        Key new_key{old_key.X_proc, old_key.X_rx, new_aos};
        rolled_rows.add_or_update(new_key, val);
    }

    // bucket rolled rows by (X_proc, X_rx)
    static std::vector<std::pair<Key,double>> buckets[NUM_STATES][NUM_STATES];
    for (int i = 0; i < NUM_STATES; i++) {
        for (int j = 0; j < NUM_STATES; j++) {
            buckets[i][j].clear();
        }
    }
    for (const auto &rolled_entry : rolled_rows.get_table()) {
        buckets[rolled_entry.first.X_proc][rolled_entry.first.X_rx].push_back(rolled_entry);
    }


    // identify non-zero belief states
    uint8_t nz_prev[NUM_STATES], nz_prev_n = 0;
    uint8_t nz_cur [NUM_STATES], nz_cur_n  = 0;
    for (uint8_t i = 0; i < NUM_STATES; i++) {
        if (prev_belief[i] > EPSILON) nz_prev[nz_prev_n++] = i;
        if (belief[i] > EPSILON) nz_cur [nz_cur_n++] = i;
    }

    // update AoS table
    for (uint8_t pi = 0; pi < nz_prev_n; pi++) {
        uint8_t other = nz_prev[pi];
        double prev_p = prev_belief[other];

        for (uint8_t si = 0; si < nz_cur_n; si++) {
            uint8_t s = nz_cur[si];

            for (uint8_t ci = 0; ci < nz_cur_n; ci++) {
                uint8_t state = nz_cur[ci];

                // state == s -> reset AoS
                if (state == s) {
                    Key k{state, s, 0};
                    new_table.add_or_update(k, 1.0);
                    continue;
                }

                double base;
                
                // current belief state == 1 -> use previous belief state
                if (fabs(belief[state] - 1.0) < EPSILON) {
                    base = prev_p;
                
                // otherwise compute base normally
                } else {
                    base = prev_p / belief[state] * P[other][state];
                }

                if (base == 0.0) continue;

                // roll over AoS entries
                for (const auto &rolled_entry : buckets[other][s]) {
                    const Key &rolled_key = rolled_entry.first;

                    if (rolled_key.X_proc != other || rolled_key.X_rx != s) continue;
                    
                    double incr = base * rolled_entry.second;
                    if (incr == 0.0) continue;

                    Key out_key = {state, s, rolled_key.AoII};
                    double cur = 0;
                    new_table.get(out_key, cur);
                    new_table.add_or_update(out_key, cur + incr);
                }
            }
        }
    }

    return new_table;
}

// helper matrix functions

void matrix_transpose(
    const double A[NUM_STATES][NUM_STATES], 
    double result[NUM_STATES][NUM_STATES] // output
) {
    for (int i = 0; i < NUM_STATES; i++) {
        for (int j = 0; j < NUM_STATES; j++) {
            result[j][i] = A[i][j];
        }
    }
}

void matrix_multiply(
    const double A[NUM_STATES][NUM_STATES], 
    const double B[NUM_STATES][NUM_STATES], 
    double result[NUM_STATES][NUM_STATES] // output
) {
    static double temp[NUM_STATES][NUM_STATES];
    for (int i = 0; i < NUM_STATES; i++) {
        for (int j = 0; j < NUM_STATES; j++) {
            double sum = 0.0;
            for (int k = 0; k < NUM_STATES; k++) {
                sum += A[i][k] * B[k][j];
            }
            temp[i][j] = sum;
        }
    }

    memcpy(result, temp, sizeof(temp));
}

void matrix_power(
    const double A[NUM_STATES][NUM_STATES], 
    int power, 
    double result[NUM_STATES][NUM_STATES] // output
) {
    // initialize result as identity matrix
    for (int i = 0; i < NUM_STATES; i++) {
        for (int j = 0; j < NUM_STATES; j++) {
            result[i][j] = (i == j) ? 1.0 : 0.0;
        }
    }
    
    static double temp[NUM_STATES][NUM_STATES];
    static double current[NUM_STATES][NUM_STATES];
    memcpy(current, A, sizeof(double) * NUM_STATES * NUM_STATES);
    
    // iterative exponentiation by squaring
    while (power > 0) {
        if (power % 2 == 1) {
            matrix_multiply(result, current, temp);
            memcpy(result, temp, sizeof(double) * NUM_STATES * NUM_STATES);
        }
        matrix_multiply(current, current, temp);
        memcpy(current, temp, sizeof(double) * NUM_STATES * NUM_STATES);
        power /= 2;
    }
}
void fast_beta(
    const double powered[NUM_STATES][NUM_STATES],
    const double initial_state_probs[NUM_STATES],
    double beta[NUM_STATES][NUM_STATES]
) {
    static double power_b[NUM_STATES];
    memset(power_b, 0, sizeof(double) * NUM_STATES);
    
    // compute power_b
    for (size_t j = 0; j < NUM_STATES; j++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            power_b[j] += initial_state_probs[i] * powered[i][j];
        }
    }

    // compute beta
    for (size_t j = 0; j < NUM_STATES; j++) {
        if (fabs(power_b[j]) > EPSILON) {
            for (size_t i = 0; i < NUM_STATES; i++) {
                beta[j][i] = (powered[i][j] * initial_state_probs[i]) / power_b[j];
            }
        } else {
            for (size_t i = 0; i < NUM_STATES; i++) {
                beta[j][i] = 0.0;
            }
        }
    }
}

void _calculate_backwards_probability(
    const double initial_state_probs[NUM_STATES], 
    const double transition_matrix[NUM_STATES][NUM_STATES], 
    int num_steps, 
    double beta[NUM_STATES][NUM_STATES]
) {
    static double transposed[NUM_STATES][NUM_STATES];

    if (!cached_powered_valid[num_steps]) {
        matrix_transpose(transition_matrix, transposed);
        matrix_power(transposed, num_steps, cached_powered[num_steps]);
        cached_powered_valid[num_steps] = true;
    }

    fast_beta(cached_powered[num_steps], initial_state_probs, beta);
}

// constructor
PSBOAgent::PSBOAgent(
    GoalOrientedTensor *got,
    const double got_weight,
    const double energy_weight,
    const bool first_time_init,
    const bool verbose
) {
    this->got = got;
    this->got_weight() = got_weight;
    this->energy_weight() = energy_weight;
    this->verbose() = verbose;

    if (first_time_init) {
        this->reset(got);
    }
}

SimulationState PSBOAgent::_create_simulation_state() {
    SimulationState sim_state;

    // copy belief states
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        for (size_t state = 0; state < NUM_STATES; state++) {
            sim_state.belief_process_state[dim][state] = this->belief_process_state()[dim][state];
            sim_state.previous_belief_process_state[dim][state] = this->previous_belief_process_state()[dim][state];
            sim_state.belief_process_state_at_sender[dim][state] = this->belief_process_state()[dim][state];
            sim_state.belief_process_state_at_receiver[dim][state] = this->belief_process_state_at_receiver()[dim][state];
        }
    }
    
    // set AoI
    sim_state.aoi_sender = 0; // because we just sensed
    sim_state.aoi_receiver = this->aoi_receiver();
    
    // copy transmissions
    sim_state.simulation_state_transmissions.size = this->transmissions().size;
    for (size_t i = 0; i < this->transmissions().size && i < MAX_TRANSMISSIONS; i++) {
        sim_state.simulation_state_transmissions.data[i] = this->transmissions().data[i];
    }
    
    // copy AoS tables
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        AoSTable::copy(this->belief_aos_table[dim], sim_state.belief_aos_table[dim]);
    }
    
    return sim_state;
}


double PSBOAgent::simulate(
    SimulationState &current_simulation_state, 
    const uint8_t action, 
    SimulationState &next_simulation_state
) {
    uint8_t planned_sleep_duration = action;
    uint8_t aoi_receiver = current_simulation_state.aoi_receiver;
    uint8_t aoi_sender = current_simulation_state.aoi_sender;
    double got_cost = 0.0;

    // use agent buffers as working space to avoid stack usage
    double (*belief_process_state)[NUM_STATES] = this->sim_belief_process_state;
    double (*previous_belief_process_state)[NUM_STATES] = this->sim_previous_belief_process_state;
    double (*belief_process_state_at_receiver)[NUM_STATES] = this->sim_belief_process_state_at_receiver;
    auto &simulation_state_transmissions = this->sim_simulation_state_transmissions;
    auto &belief_aos_table = this->sim_belief_aos_table;
    auto &process_state_at_receiver_dependent_on_process_state = this->sim_process_state_rx_dep;

    // copy belief states and AoS tables from current_simulation_state
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        for (size_t state = 0; state < NUM_STATES; state++) {
            belief_process_state[dim][state] = 
                current_simulation_state.belief_process_state[dim][state];
            previous_belief_process_state[dim][state] = 
                current_simulation_state.previous_belief_process_state[dim][state];
            belief_process_state_at_receiver[dim][state] = 
                current_simulation_state.belief_process_state_at_receiver[dim][state];
        }
        
        AoSTable::copy( 
            current_simulation_state.belief_aos_table[dim], 
            belief_aos_table[dim]
        );
    }
    
    // initialize simulation_state_transmissions as empty
    simulation_state_transmissions.size = 0;
    
    // main simulation loop
    for (size_t step = 0; step < planned_sleep_duration; step++) {
        TransmissionList new_transmissions;
        new_transmissions.size = 0;
        
        // transmissions loop
        for (size_t i = 0; i < current_simulation_state.simulation_state_transmissions.size && i < MAX_TRANSMISSIONS; i++) {
            Transmission transmission = current_simulation_state.simulation_state_transmissions.data[i];
            
            // transmission arrived -> update belief at receiver and remove transmission
            if (transmission.time_until_reception == 0) {
                for (size_t dim = 0; dim < NUM_DIMS; dim++) {
                    for (size_t state = 0; state < NUM_STATES; state++) {
                        belief_process_state_at_receiver[dim][state] = 
                            transmission.belief_state[dim][state];
                    }
                }
                aoi_receiver = transmission.aoi;

            // otherwise decrement time until reception and increment AoI
            } else {
                transmission.time_until_reception--;
                transmission.aoi = transmission.aoi == MAX_AOI ? MAX_AOI : transmission.aoi + 1;
                if (new_transmissions.size < MAX_TRANSMISSIONS) {
                    new_transmissions.data[new_transmissions.size++] = transmission;
                }
            }
        }
        
        // update transmissions
        simulation_state_transmissions = new_transmissions;
        
        // belief update loop
        if (strcmp(this->got->get_metric(), "aos") == 0) {
            for (size_t dim = 0; dim < NUM_DIMS; dim++) {
                AoSTable new_table = static_update_belief_aos(
                    belief_aos_table[dim],
                    belief_process_state[dim],
                    previous_belief_process_state[dim],
                    this->estimated_process_transition_probabilities()[dim],
                    aoi_receiver
                );
                AoSTable::copy(new_table, belief_aos_table[dim]);
            }
        }
        
        // compute process state at receiver dependent on process state
        for (size_t dim = 0; dim < NUM_DIMS; dim++) {
            _calculate_backwards_probability(
                belief_process_state_at_receiver[dim],
                this->estimated_process_transition_probabilities()[dim],
                aoi_receiver,
                process_state_at_receiver_dependent_on_process_state[dim]
            );
        }
        
        // compute GoT cost
        double exact_got_cost = this->got->get_belief_cost(
            belief_process_state,
            process_state_at_receiver_dependent_on_process_state,
            aoi_receiver,
            belief_aos_table
        );
        got_cost += exact_got_cost;
        
        // copy belief_process_state to previous_belief_process_state
        for (size_t dim = 0; dim < NUM_DIMS; dim++) {
            for (size_t state = 0; state < NUM_STATES; state++) {
                previous_belief_process_state[dim][state] = belief_process_state[dim][state];
            }
        }
        
        // update belief process state
        for (size_t dim = 0; dim < NUM_DIMS; dim++) {
            this->_general_belief_process_state_update(belief_process_state[dim], dim, belief_process_state[dim]);
        }
        
        // increment AoI
        aoi_sender = aoi_sender == MAX_AOI ? MAX_AOI : aoi_sender + 1;
        aoi_receiver = aoi_receiver == MAX_AOI ? MAX_AOI : aoi_receiver + 1;
    }
    
    // compute energy and total cost
    double energy_cost = planned_sleep_duration * this->deep_sleep_energy();
    double cost = this->energy_weight() * energy_cost + this->got_weight() * got_cost;
    
    // prepare next_simulation_state
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        for (size_t state = 0; state < NUM_STATES; state++) {
            next_simulation_state.belief_process_state[dim][state] = belief_process_state[dim][state];
            next_simulation_state.previous_belief_process_state[dim][state] = previous_belief_process_state[dim][state];
            next_simulation_state.belief_process_state_at_sender[dim][state] = 
                current_simulation_state.belief_process_state_at_sender[dim][state];
            next_simulation_state.belief_process_state_at_receiver[dim][state] = 
                belief_process_state_at_receiver[dim][state];
        }
        AoSTable::copy(belief_aos_table[dim], next_simulation_state.belief_aos_table[dim]);
    }
    next_simulation_state.aoi_sender = aoi_sender;
    next_simulation_state.aoi_receiver = aoi_receiver;
    next_simulation_state.simulation_state_transmissions.size = simulation_state_transmissions.size;
    for (size_t i = 0; i < simulation_state_transmissions.size && i < MAX_TRANSMISSIONS; i++) {
        next_simulation_state.simulation_state_transmissions.data[i] = simulation_state_transmissions.data[i];
    }

    return cost;
}

uint8_t PSBOAgent::_estimate_delay() {
    return 0;
}

void PSBOAgent::_reset_estimated_process_transition_probabilities() {
    uint16_t init_total = 1000;
    uint16_t init_returns = init_total - 1;
    
    // for each dimension
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        // find max and min states
        int16_t max_state = INT16_MIN;
        int16_t min_state = INT16_MAX;
        for (size_t i = 0; i < NUM_STATES; i++) {
            int16_t state_val = (*this->got->get_states())[dim][i];
            max_state = max_state < state_val ? state_val : max_state;
            min_state = min_state > state_val ? state_val : min_state;
        }

        // initialize counts and probabilities
        for (size_t i = 0; i < NUM_STATES; i++) {
            int16_t state_i = (*this->got->get_states())[dim][i];
            for (size_t j = 0; j < NUM_STATES; j++) {
                int16_t state_j = (*this->got->get_states())[dim][j];
                
                // equal states -> high probability
                if (state_i == state_j) {
                    this->process_transition_counts()[dim][i][j] = init_returns;
                    this->estimated_process_transition_probabilities()[dim][i][j] = 
                        (double)init_returns / init_total;

                // boundary states -> only one direction
                } else if ((state_i == max_state && state_j == state_i - 1) || 
                          (state_i == min_state && state_j == state_i + 1)) {
                    this->process_transition_counts()[dim][i][j] = init_total - init_returns;
                    this->estimated_process_transition_probabilities()[dim][i][j] = 
                        (double)(init_total - init_returns) / init_total;
                
                // adjacent states -> medium probability
                } else if (std::abs(state_i - state_j) == 1) {
                    this->process_transition_counts()[dim][i][j] = (init_total - init_returns) / 2.0;
                    this->estimated_process_transition_probabilities()[dim][i][j] = 
                        (double)(1 - (double)init_returns / init_total) / 2.0;

                // non-adjacent states -> zero probability
                } else {
                    this->process_transition_counts()[dim][i][j] = 0.0;
                    this->estimated_process_transition_probabilities()[dim][i][j] = 0.0;
                }
            }
        }
    }
}

double PSBOAgent::_estimate_drop_rate() {
    if (this->total_attempts() > 0) {
        double success_rate = (double)this->total_transmissions() / this->total_attempts();
        return 1.0 - success_rate;
    } else {
        return 0.00;
    }
}

double PSBOAgent::_estimate_antenna_energy(const bool deep_sleep_in_next_step) {
    double drop_rate = this->_estimate_drop_rate();
    
    // calculate average antenna power interval
    double avg_antenna_power_interval_1 = (1.0 - drop_rate) * this->avg_delay_tx_to_feedback_rx_in_ms() + 
    drop_rate * (double)SCAN_INTERVAL_MS / (1.0 - drop_rate);
    double avg_antenna_power_interval_2 = TIME_STEP_MS - SENSING_INTERVAL_MS - WAKE_UP_INTERVAL_MS;
    double avg_antenna_power_interval = avg_antenna_power_interval_1 < avg_antenna_power_interval_2 ? avg_antenna_power_interval_1 : avg_antenna_power_interval_2;

    // calculate remaining time power consumption
    double remaining_time_power = 0;
    if (deep_sleep_in_next_step) {
        remaining_time_power = DEEP_SLEEP_MJ_PER_MS;
    } else {
        remaining_time_power = IDLE_MJ_PER_MS;
    }

    // return total energy consumption
    return avg_antenna_power_interval * ANTENNA_MJ_PER_MS + (TIME_STEP_MS - SENSING_INTERVAL_MS - WAKE_UP_INTERVAL_MS - avg_antenna_power_interval) * remaining_time_power;
}

void PSBOAgent::_general_belief_process_state_update(
    const double belief_process_state[NUM_STATES],
    const size_t dim,
    double updated_belief_process_state[NUM_STATES]
) {
    if (dim >= NUM_DIMS) throw std::out_of_range("Dimension index out of range");

    // compute new belief
    for (size_t j = 0; j < NUM_STATES; j++) {
        double sum = 0.0;
        for (size_t i = 0; i < NUM_STATES; i++) {
            if (fabs(belief_process_state[i]) > EPSILON) {
                sum += belief_process_state[i] * this->estimated_process_transition_probabilities()[dim][i][j];
            }
        }
        updated_belief_process_state[j] = sum;
    }

    // prune small values
    double total = 0.0;
    for (size_t i = 0; i < NUM_STATES; i++) {
        if (updated_belief_process_state[i] < EPSILON) {
            updated_belief_process_state[i] = 0.0;
        }
        total += updated_belief_process_state[i];
    }

    // normalize belief
    if (total > EPSILON) {
        const double inv_total = 1.0 / total;
        for (size_t i = 0; i < NUM_STATES; i++) {
            updated_belief_process_state[i] *= inv_total;

            // prune again after normalization
            if (updated_belief_process_state[i] < EPSILON) {
                updated_belief_process_state[i] = 0.0;
            }
        }
    } else {
        // total ~ 0 -> everything zero
        for (size_t i = 0; i < NUM_STATES; i++) {
            updated_belief_process_state[i] = 0.0;
        }
    }
}

void PSBOAgent::_update_belief_process_state(const size_t dim) {
    if (dim >= NUM_DIMS) throw std::out_of_range("Dimension index out of range");

    // store previous belief
    for (size_t i = 0; i < NUM_STATES; i++) {
        this->previous_belief_process_state()[dim][i] = this->belief_process_state()[dim][i];
    }
    
    // update belief
    this->_general_belief_process_state_update(this->belief_process_state()[dim], dim, this->belief_process_state()[dim]);
}

void PSBOAgent::_update_process_state_at_sender(const int16_t process_state[NUM_DIMS]) {
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->process_state_at_sender()[dim] = process_state[dim];
    }
}

void PSBOAgent::_update_belief_process_state_after_sensing(const int16_t process_state[NUM_DIMS]) {
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        // reset all states to 0
        for (size_t i = 0; i < NUM_STATES; i++) {
            this->belief_process_state()[dim][i] = 0.0;
        }
        
        // set current state to 1
        const size_t index_current_state = state_to_index((*this->got->get_states())[dim], process_state[dim]);
        if (index_current_state < NUM_STATES) {
            this->belief_process_state()[dim][index_current_state] = 1.0;
        }
    }
}

void PSBOAgent::_update_belief_process_state_at_receiver() {
    // transmission arrives -> update belief state at receiver
    for (size_t idx = 0; idx < this->transmissions().size; idx++) {
        if (this->transmissions().data[idx].time_until_reception == 0) {
            for (size_t dim=0; dim<NUM_DIMS; dim++) {
                for (size_t s=0; s<NUM_STATES; s++) {
                    this->belief_process_state_at_receiver()[dim][s] =
                    this->transmissions().data[idx].belief_state[dim][s];
                }
            }
        }
    }
}

void PSBOAgent::_update_aoi_receiver() {
    this->aoi_receiver() = this->aoi_receiver() == MAX_AOI ? MAX_AOI : this->aoi_receiver() + 1;

    for (size_t idx = 0; idx < this->transmissions().size && idx < MAX_TRANSMISSIONS; idx++) {
        Transmission &transmission = this->transmissions().data[idx];
        if (transmission.time_until_reception == 0) {
            this->aoi_receiver() = transmission.aoi;
        }
    }
}

void PSBOAgent::_update_belief_aos() {
    if (strcmp(this->got->get_metric(), "aos") != 0) {
        return;
    }

    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        AoSTable new_table = static_update_belief_aos(
            this->belief_aos_table[dim],
            this->belief_process_state()[dim],
            this->previous_belief_process_state()[dim],
            this->estimated_process_transition_probabilities()[dim],
            this->aoi_receiver()
        );
        
        AoSTable::copy(new_table, this->belief_aos_table[dim]);
    }
}

void PSBOAgent::_add_transmissions(const int feedback) {
    if (feedback < 0) { // no feedback received -> do nothing
        return;
    }

    uint8_t aoi_sender = 0; // because the device senses every time it wakes up

    // create new transmission
    Transmission new_transmission;
    new_transmission.time_until_reception = this->_estimate_delay();
    new_transmission.aoi = aoi_sender;

    // update belief state (one-hot encoding of current state)
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        int16_t current_state_val = this->process_state_at_sender()[dim];
        size_t current_state_idx = state_to_index((*this->got->get_states())[dim], current_state_val);
        for (size_t i = 0; i < NUM_STATES; i++) {
            new_transmission.belief_state[dim][i] = (i == current_state_idx) ? 1.0 : 0.0;
        }
    }

    // add new transmission to the list
    if (this->transmissions().size >= MAX_TRANSMISSIONS) {
        throw std::runtime_error("Cannot add transmission - list full");
    }
    this->transmissions().data[this->transmissions().size] = new_transmission;
    this->transmissions().size++;
    this->total_transmissions()++;
}

void PSBOAgent::_update_transmissions() {
    if (this->transmissions().size == 0) return;
    
    // update time_until_reception and AoI for all transmissions
    size_t write = 0;
    for (size_t i = 0; i < this->transmissions().size; i++) {
        Transmission &trans = this->transmissions().data[i];
        trans.time_until_reception--;
        trans.aoi = (trans.aoi < MAX_AOI - 1) ? trans.aoi + 1 : MAX_AOI;
        if (trans.time_until_reception >= 0) {
            this->transmissions().data[write++] = trans; // keep only pending transmissions
        }
    }

    this->transmissions().size = write;
}

void PSBOAgent::_update_estimated_process_transition_matrix(const int16_t process_state[NUM_DIMS]) {
    if (this->process_state_at_sender()[0] == INT16_MAX) {
        return;
    }

    // compare old and new process states
    bool equal = true;
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        if (process_state[dim] != this->process_state_at_sender()[dim]) {
            equal = false;
            break;
        }
    }
    
    // update transition counts
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        const size_t index_old_state = state_to_index((*this->got->get_states())[dim], 
                                                     this->process_state_at_sender()[dim]);
        const size_t index_new_state = state_to_index((*this->got->get_states())[dim], 
                                                     process_state[dim]);
        
        if (equal) {
            this->process_transition_counts()[dim][index_old_state][index_old_state] += 
                this->last_planned_sleep_duration() + 1;
        } else {
            int transitions_in_old_state = this->last_planned_sleep_duration() / 2;
            int transitions_in_new_state = this->last_planned_sleep_duration() - transitions_in_old_state;
            
            this->process_transition_counts()[dim][index_old_state][index_old_state] += transitions_in_old_state;
            this->process_transition_counts()[dim][index_old_state][index_new_state] += 1;
            this->process_transition_counts()[dim][index_new_state][index_new_state] += transitions_in_new_state;
        }
    }

    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        // compute row sums
        static double row_sums[NUM_STATES];
        memset(row_sums, 0, sizeof(double) * NUM_STATES);
        for (size_t i = 0; i < NUM_STATES; i++) {
            for (size_t j = 0; j < NUM_STATES; j++) {
                row_sums[i] += this->process_transition_counts()[dim][i][j];
            }
        }
        
        // update estimated transition probabilities
        for (size_t i = 0; i < NUM_STATES; i++) {
            if (row_sums[i] > 0) {
                for (size_t j = 0; j < NUM_STATES; j++) {
                    this->estimated_process_transition_probabilities()[dim][i][j] = 
                        this->process_transition_counts()[dim][i][j] / row_sums[i];
                }
            }
        }
    }
    memset(cached_powered_valid, 0, sizeof(cached_powered_valid));
}

double PSBOAgent::_get_sending_probability(
    const SimulationState *current_sim_state
) {
    static double beta[NUM_STATES][NUM_STATES];
    
    double total_sending_prob = 1.0;

    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        _calculate_backwards_probability(
            current_sim_state->belief_process_state_at_receiver[dim],
            this->estimated_process_transition_probabilities()[dim],
            current_sim_state->aoi_receiver,
            beta
        );

        double trace = 0.0;
        for (size_t i = 0; i < NUM_STATES; i++) {
            trace += current_sim_state->belief_process_state[dim][i] * beta[i][i];
        }

        double dim_prob = 1.0 - trace;
        total_sending_prob *= (1.0 - dim_prob);
    }

    double final_prob = 1.0 - total_sending_prob;
    if (fabs(current_sim_state->belief_process_state[0][0] - 1.0) < EPSILON) {
        _calculate_backwards_probability(
            current_sim_state->belief_process_state_at_receiver[0],
            this->estimated_process_transition_probabilities()[0],
            current_sim_state->aoi_receiver,
            beta
        );
    }

    return final_prob;
}

bool PSBOAgent::_antenna_decision() {

    if (strcmp(this->got->get_metric(), "aoi") == 0) {
        return true; // always transmit for AoI metric
    }

    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        for (size_t state = 0; state < NUM_STATES; state++) {
            if (this->belief_process_state()[dim][state] != this->belief_process_state_at_receiver()[dim][state]) {
                return true; // beliefs differ -> transmit
            }
        }
    }

    return false; // all beliefs match
}

uint8_t PSBOAgent::_sleeping_decision() {
    if (this->antenna_decision && this->transmissions().size == 0) {
        return 0; // need to transmit, no pending transmissions
    }

    SimulationState current_sim_state = this->_create_simulation_state();
    double antenna_energy = this->_estimate_antenna_energy(true);
    double antenna_energy_when_not_sleeping = this->_estimate_antenna_energy(false);
    double drop_rate = this->_estimate_drop_rate();
    double success_rate = 1.0 - drop_rate;
    double avg_number_of_tx_time_steps = 1.0 / success_rate;
    int tx_horizon = 2 * (int)avg_number_of_tx_time_steps;
    int step_count = 0;

    // static arrays to avoid stack overflow
    static double sending_probabilities[2 * MAX_TRANSMISSIONS + MAX_AOI + 5] = {0};
    static double costs_per_step[2 * MAX_TRANSMISSIONS + MAX_AOI + 5] = {0};
    memset(sending_probabilities, 0, sizeof(sending_probabilities));
    memset(costs_per_step, 0, sizeof(costs_per_step));

    // warm-up phase
    SimulationState next_state;
    for (size_t tx_step = 0; tx_step < tx_horizon; ++tx_step) {
        double total_cost_from_simulate = this->simulate(
            current_sim_state,
            this->sleep_step_length(),
            next_state
        );
        current_sim_state = next_state;
        costs_per_step[step_count] = total_cost_from_simulate;
        sending_probabilities[step_count] = this->_get_sending_probability(&current_sim_state);
        step_count++;
    }

    double previous_avg_cost = FLT_MAX;
    uint8_t best_duration = 0;
    
    // 2) Main loop
    while (best_duration < MAX_AOI) {
        double total_cost_from_simulate = this->simulate(
            current_sim_state,
            this->sleep_step_length(),
            next_state
        );
        if (step_count >= (int)(sizeof(costs_per_step)/sizeof(costs_per_step[0]) - 1)) {
            break;
        }
        current_sim_state = next_state;
        costs_per_step[step_count] = total_cost_from_simulate;
        sending_probabilities[step_count] = this->_get_sending_probability(&current_sim_state);
        step_count++;
        
        // calculate candidate average cost for best_duration
        double sending_probability = sending_probabilities[best_duration];
        double total_time = (double)best_duration;
        double candidate_avg_cost = 0.0;
        if (best_duration > 0) {
            double sum = 0.0;
            for (int j = 0; j < best_duration; ++j) {
                sum += costs_per_step[j];
            }
            candidate_avg_cost = sum / total_time;
        }
        
        // consider retransmissions
        double e_cost = 0.0;
        if (best_duration == 0) {
            e_cost += this->energy_weight() * (antenna_energy_when_not_sleeping - antenna_energy);
        }
        for (int tx_step = 0; tx_step < tx_horizon; ++tx_step) {
            double p = success_rate * pow(1.0 - success_rate, (double)tx_step);
            if (p <= 0) continue;
            
            e_cost += this->energy_weight() *
                     (this->sensing_energy() +
                      sending_probability * antenna_energy +
                      this->wake_up_energy());
            
            int idx = best_duration + tx_step;
            if (idx >= step_count) {
                break;
            }
            
            double got_cost = costs_per_step[idx];
            
            double numerator = total_time * candidate_avg_cost + p * (got_cost + e_cost);
            double denominator = total_time + p;
            candidate_avg_cost = numerator / denominator;
            total_time += p;
        }
        
        // check for cost increase
        double diff = candidate_avg_cost - previous_avg_cost;
        if (candidate_avg_cost > previous_avg_cost && best_duration > 0) {
            best_duration -= this->sleep_step_length();
            break;
        }
        
        previous_avg_cost = candidate_avg_cost;
        best_duration += this->sleep_step_length();
    }
    
    return best_duration < MAX_AOI ? best_duration : MAX_AOI;
}

bool PSBOAgent::antenna_action(const EnvState env_state[NUM_DIMS]) {
    // update states for the duration of the last sleep
    for (size_t i = 0; i < this->last_planned_sleep_duration(); i++) {
        for (size_t dim = 0; dim < NUM_DIMS; dim++) {
            this->_update_belief_process_state(dim);
        }
        this->_update_belief_process_state_at_receiver();
        this->_update_aoi_receiver();
        this->_update_belief_aos();
        this->_update_transmissions();
    }

    int16_t process_state[NUM_DIMS];
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        process_state[dim] = env_state[dim].X_proc;
    }

    // more updates after sensing
    this->_update_estimated_process_transition_matrix(process_state);
    this->_update_process_state_at_sender(process_state);
    this->_update_belief_process_state_after_sensing(process_state);

    // decide on antenna action
    this->antenna_decision = this->_antenna_decision();

    return this->antenna_decision;
}

uint8_t PSBOAgent::sleep_action(const EnvState env_state[NUM_DIMS]) {
    this->last_planned_sleep_duration() = this->_sleeping_decision(); // update last planned sleep duration

    // perform updates for the duration of the planned sleep
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->_update_belief_process_state(dim);
    }
    this->_update_belief_process_state_at_receiver();
    this->_update_aoi_receiver();
    this->_update_belief_aos();
    this->_update_transmissions();

    return this->last_planned_sleep_duration();
}

void PSBOAgent::update_after_sensing_step(const EnvState env_state[NUM_DIMS]) {
}

void PSBOAgent::update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) {
    if (feedback < 0) {
        this->total_attempts() += MAX_TRANSMISSIONS; // might vary depending on implementation, need to change accordingly
    } else {
        this->total_attempts() += feedback;
    }
    this->_add_transmissions(feedback);

    this->avg_delay_tx_to_feedback_rx_in_ms()  = this->avg_delay_tx_to_feedback_rx_in_ms() * (1 - 1.0/this->total_transmissions()) + transmission_time_ms * 1.0/this->total_transmissions();
}

void PSBOAgent::update_after_sleep_step(const EnvState env_state[NUM_DIMS]) {
}

void PSBOAgent::reset(GoalOrientedTensor *got) {
    this->got = got;

    this->sensing_energy() = SENSING_MJ_PER_MS * SENSING_INTERVAL_MS;
    this->wake_up_energy() = WAKE_UP_MJ_PER_MS * WAKE_UP_INTERVAL_MS;
    this->deep_sleep_energy() = DEEP_SLEEP_MJ_PER_MS * TIME_STEP_MS;

    this->last_planned_sleep_duration() = 0;
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->process_state_at_sender()[dim] = INT16_MAX;
        size_t index_initial_state = 0;
        for (size_t i = 0; i < NUM_STATES; i++) {
            double val = i == index_initial_state ? 1.0 : 0.0;
            this->belief_process_state()[dim][i] = val;
            this->previous_belief_process_state()[dim][i] = val;
            this->belief_process_state_at_receiver()[dim][i] = val;
        }
    }

    this->transmissions().size = 0;

    this->_reset_estimated_process_transition_probabilities();

    this->aoi_receiver() = 0;
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        this->belief_aos_table[dim].clear();
        for (int16_t i = 0; i < NUM_STATES; i++) {
            for (int16_t j = 0; j < NUM_STATES; j++) {
               Key key = {i, j, 0};
                this->belief_aos_table[dim].add_or_update(key, 1.0);
            }
        }
    }


   this->total_attempts() = 0;
   this->total_transmissions() = 0;
}

void PSBOAgent::_recompute_transition_matrix_from_counts() {
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        // compute row sums
        for (size_t i = 0; i < NUM_STATES; i++) {
            double row_sum = 0.0;
            for (size_t j = 0; j < NUM_STATES; j++) {
                row_sum += this->process_transition_counts()[dim][i][j];
            }

            if (row_sum > 0.0) {
                double inv = 1.0 / row_sum;
                for (size_t j = 0; j < NUM_STATES; j++) {
                    this->estimated_process_transition_probabilities()[dim][i][j] =
                        this->process_transition_counts()[dim][i][j] * inv;
                }
            } else {
                // no transitions observed -> uniform distribution
                double uniform = 1.0 / NUM_STATES;
                for (size_t j = 0; j < NUM_STATES; j++) {
                    this->estimated_process_transition_probabilities()[dim][i][j] = uniform;
                }
            }
        }
    }
}

void PSBOAgent::save_to_nvs(const nvs_handle_t nvs_handle) {
    // save main struct
    size_t agent_data_size = sizeof(PSBOAgentData);
    esp_err_t err = nvs_set_blob(nvs_handle, "agent_data", &this->psbo_data, agent_data_size);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] Failed to save agent data to NVS: %d\n", err);
        return;
    }

    // save each belief_aos_table
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        // serialize AoSTable
        size_t len;
        uint8_t *data = this->belief_aos_table[dim].serialize(len);
        if (len == 0 || data == nullptr) {
            continue;
        }
        
        // save to NVS
        char key[32];
        snprintf(key, sizeof(key), "aos_table_%zu", dim);
        err = nvs_set_blob(nvs_handle, key, data, len);
        free(data);
        if (err != ESP_OK) {
            Serial.printf("[ERROR] Failed to save aos_table_%zu to NVS: %d\n", dim, err);
        }
    }

    // save transition counts
    err = nvs_set_blob(nvs_handle, "tr_counts", 
             this->process_transition_counts(),
             sizeof(this->process_transition_counts()));

    if (err != ESP_OK) {
        Serial.printf("[ERROR] Failed to save transition_counts: %d\n", err);
    }

    // commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] Failed to commit NVS: %d\n", err);
    }
}

void PSBOAgent::load_from_nvs(const nvs_handle_t nvs_handle) {
    // load main struct
    size_t required_size = sizeof(PSBOAgentData);
    esp_err_t err = nvs_get_blob(nvs_handle, "agent_data", &this->psbo_data, &required_size);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] Failed to load agent data from NVS: %d\n", err);
        return;
    }

    // load each belief_aos_table
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
        char key[32];
        snprintf(key, sizeof(key), "aos_table_%zu", dim);

        // get required length
        size_t len = 0;
        err = nvs_get_blob(nvs_handle, key, NULL, &len);
        if (err == ESP_ERR_NVS_NOT_FOUND || len == 0) {
            this->belief_aos_table[dim].clear();
            continue;
        }
        if (err != ESP_OK) {
            Serial.printf("[ERROR] Failed to get aos_table_%zu size: %d\n", dim, err);
            continue;
        }

        // allocate buffer
        uint8_t *buffer = (uint8_t*)malloc(len);
        if (buffer == NULL) {
            Serial.printf("[ERROR] Failed to allocate memory for aos_table_%zu\n", dim);
            continue;
        }

        // read data
        err = nvs_get_blob(nvs_handle, key, buffer, &len);
        if (err != ESP_OK) {
            Serial.printf("[ERROR] Failed to load aos_table_%zu: %d\n", dim, err);
            free(buffer);
            continue;
        }

        // deserialize
        this->belief_aos_table[dim].deserialize(buffer, len);
        
        free(buffer);
    }

    // load transition counts
    size_t cnt_size = sizeof(this->process_transition_counts());
    err = nvs_get_blob(nvs_handle, "tr_counts",
                    this->process_transition_counts(),
                    &cnt_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // first boot -> initialize counts to zero
        memset(this->process_transition_counts(), 0, cnt_size);
    }
    else if (err != ESP_OK) {
        Serial.printf("[ERROR] Failed to load transition_counts: %d\n", err);
    }
    this->_recompute_transition_matrix_from_counts();
}
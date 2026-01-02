#pragma once

#include "agent.h"
#include <algorithm>

typedef struct {
    int32_t time_until_reception;
    uint8_t aoi;
    double belief_state[NUM_DIMS][NUM_STATES];
} Transmission;

typedef struct {
    Transmission data[MAX_TRANSMISSIONS];
    size_t size = 0;
} TransmissionList;

typedef struct {
    double belief_process_state[NUM_DIMS][NUM_STATES];
    double previous_belief_process_state[NUM_DIMS][NUM_STATES];
    double belief_process_state_at_sender[NUM_DIMS][NUM_STATES];
    double belief_process_state_at_receiver[NUM_DIMS][NUM_STATES];
    uint8_t aoi_sender;
    uint8_t aoi_receiver;
    AoSTable belief_aos_table[NUM_DIMS];
    TransmissionList simulation_state_transmissions;
} SimulationState;

typedef struct {
    bool verbose;
    double got_weight;
    double energy_weight;
    double sensing_energy;
    double wake_up_energy;
    double deep_sleep_energy;
    uint8_t sleep_step_length = 1;
    uint8_t last_planned_sleep_duration;
    int16_t process_state_at_sender[NUM_DIMS];
    double belief_process_state[NUM_DIMS][NUM_STATES];
    double previous_belief_process_state[NUM_DIMS][NUM_STATES];
    double belief_process_state_at_receiver[NUM_DIMS][NUM_STATES];
    uint8_t aoi_receiver;
    TransmissionList transmissions;
    bool antenna_decision = false;
    uint8_t total_attempts = 0;
    uint8_t total_transmissions = 0;
    double process_transition_counts[NUM_DIMS][NUM_STATES][NUM_STATES];
    double estimated_process_transition_probabilities[NUM_DIMS][NUM_STATES][NUM_STATES];
    uint32_t transition_counts[NUM_DIMS][NUM_STATES][NUM_STATES];
    time_t avg_delay_tx_to_feedback_rx_in_ms = 144 + 2 * 51.4; // gets updated 
}PSBOAgentData;

class PSBOAgent : public Agent {
public:
    // constructor
    PSBOAgent(
        GoalOrientedTensor *got,
        const double got_weight,
        const double energy_weight,
        const bool first_time_init,
        const bool verbose
    );
    
    // decision methods
    bool antenna_action(const EnvState env_state[NUM_DIMS]);
    uint8_t sleep_action(const EnvState env_state[NUM_DIMS]);
    
    // update methods
    void update_after_sensing_step(const EnvState env_state[NUM_DIMS]);
    void update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms);
    void update_after_sleep_step(const EnvState env_state[NUM_DIMS]);

    // persistence methods
    void save_to_nvs(const nvs_handle_t nvs_handle);
    void load_from_nvs(const nvs_handle_t nvs_handle);
    
    // reset method
    void reset(GoalOrientedTensor *got);

private:
    GoalOrientedTensor *got;

    // non-persistent data
    bool antenna_decision = false;
    
    // simulate() working buffers
    double sim_belief_process_state[NUM_DIMS][NUM_STATES];
    double sim_previous_belief_process_state[NUM_DIMS][NUM_STATES];
    double sim_belief_process_state_at_receiver[NUM_DIMS][NUM_STATES];
    double sim_process_state_rx_dep[NUM_DIMS][NUM_STATES][NUM_STATES];
    AoSTable sim_belief_aos_table[NUM_DIMS];
    TransmissionList sim_simulation_state_transmissions;

    // agent data
    PSBOAgentData psbo_data;
    
    // belief AoS tables
    AoSTable belief_aos_table[NUM_DIMS];

    // decision methods
    bool _antenna_decision();
    uint8_t _sleeping_decision();

    // update methods
    void _general_belief_process_state_update(
        const double belief_process_state[NUM_STATES],
        const size_t dim,
        double updated_belief_process_state[NUM_STATES]
    );
    void _update_belief_process_state(const size_t dim);
    void _update_process_state_at_sender(const int16_t process_state[NUM_DIMS]);
    void _update_belief_process_state_after_sensing(const int16_t process_state[NUM_DIMS]);
    void _update_belief_process_state_at_receiver();
    void _update_aoi_receiver();
    void _update_belief_aos();
    void _update_transmissions();
    void _update_estimated_process_transition_matrix(const int16_t process_state[NUM_DIMS]);

    // simulation methods
    double simulate(
        SimulationState &current_simulation_state, 
        const uint8_t action, 
        SimulationState &next_simulation_state
    );
    void _update_simulation_state(
        SimulationState &simulation_state,
        const double belief_process_state[NUM_DIMS][NUM_STATES],
        const double previous_belief_process_state[NUM_DIMS][NUM_STATES],
        const double belief_process_state_at_sender[NUM_DIMS][NUM_STATES],
        const double belief_process_state_at_receiver[NUM_DIMS][NUM_STATES],
        const uint8_t aoi_sender,
        const uint8_t aoi_receiver,
        const TransmissionList simulation_state_transmissions,
        AoSTable belief_aos_table[NUM_DIMS]
    );
    SimulationState _create_simulation_state();

    // helper methods
    void _recompute_transition_matrix_from_counts(); // persistence helper
    void _reset_estimated_process_transition_probabilities(); // reset helper
    double _get_sending_probability(const SimulationState *current_sim_state);
    uint8_t _estimate_delay();
    double _estimate_drop_rate();
    double _estimate_antenna_energy(const bool deep_sleep_in_next_step);
    void _add_transmissions(const int feedback);

    // getters
    PSBOAgentData &data() { return psbo_data; }
    double &got_weight() { return psbo_data.got_weight; }
    double &energy_weight() { return psbo_data.energy_weight; }
    bool &verbose() { return psbo_data.verbose; }
    double &sensing_energy() { return psbo_data.sensing_energy; }
    double &wake_up_energy() { return psbo_data.wake_up_energy; }
    double &deep_sleep_energy() { return psbo_data.deep_sleep_energy; }
    int16_t *process_state_at_sender() { return psbo_data.process_state_at_sender; }
    uint8_t &sleep_step_length() { return psbo_data.sleep_step_length; }
    uint8_t &last_planned_sleep_duration() { return psbo_data.last_planned_sleep_duration; }
    double (*belief_process_state())[NUM_STATES] { return psbo_data.belief_process_state; }
    double (*previous_belief_process_state())[NUM_STATES] { return psbo_data.previous_belief_process_state; }
    double (*belief_process_state_at_receiver())[NUM_STATES] { return psbo_data.belief_process_state_at_receiver; }
    TransmissionList &transmissions() { return psbo_data.transmissions; }
    double (*estimated_process_transition_probabilities())[NUM_STATES][NUM_STATES] { return psbo_data.estimated_process_transition_probabilities; }
    uint8_t &aoi_receiver() { return psbo_data.aoi_receiver; }
    double (*process_transition_counts())[NUM_STATES][NUM_STATES] { return psbo_data.process_transition_counts; }
    uint8_t &total_attempts() { return psbo_data.total_attempts; }
    uint8_t &total_transmissions() { return psbo_data.total_transmissions; }
    time_t &avg_delay_tx_to_feedback_rx_in_ms() { return psbo_data.avg_delay_tx_to_feedback_rx_in_ms; }
};
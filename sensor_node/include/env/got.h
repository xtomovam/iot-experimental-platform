#pragma once

#include "common.h"
#include "aos_table.h"

#include <string>
#include "esp_random.h"
#include <array>

using CostFunction = std::function<double(int, int, int)>;
typedef CostFunction CostFunctionArray[NUM_DIMS];
typedef int16_t StatesType[NUM_DIMS][NUM_STATES];

/*
A class representing a Goal Oriented Tensor (GoT) for evaluating costs based on Age of Information (AoI),
Age of Synchronization (AoS), Mean Squared Error (MSE), or a generalized random cost function.
*/
class GoalOrientedTensor {
public:
    // constructor
    GoalOrientedTensor(
        const char metric[], // metric type: "aoi", "aos", "mse", "general", "specific"
        const int16_t states[NUM_DIMS][NUM_STATES],
        const double base_values[NUM_DIMS][NUM_STATES][NUM_STATES],
        const double growths[NUM_DIMS][NUM_STATES][NUM_STATES],
        const double mse_scaling // -1 -> default
    );
    
    // compute GoT cost
    double get_cost(
        const int16_t process_state[NUM_DIMS],
        const int16_t receiver_state[NUM_DIMS],
        const uint8_t aoi_receiver, // suppose all dimensions have the same AoI_rx
        const uint8_t aos // suppose all dimensions have the same AoS
    ) const;

    // compute GoT belief cost
    double get_belief_cost(
        const double belief_process_state[NUM_DIMS][NUM_STATES],
        const double belief_process_state_at_receiver[NUM_DIMS][NUM_STATES][NUM_STATES],
        const uint32_t aoi_receiver, // suppose all dimensions have the same AoI_rx
        AoSTable belief_aos_table[NUM_DIMS]
    ) const;

    // getters and setters
    const char *get_metric() const;
    StatesType *get_states();

    
    private:
    int16_t states[NUM_DIMS][NUM_STATES]; // possible states for each dimension
    double base_values[NUM_DIMS][NUM_STATES][NUM_STATES];
    double growths[NUM_DIMS][NUM_STATES][NUM_STATES];
    double mse_scaling;
    char metric[MAX_METRIC_NAME_LEN]; // metric type: "aoi", "aos", "mse", "general", "specific"
    CostFunctionArray cost_tensor;
    
    CostFunction make_cost_function(size_t dim);
    void generate_tensor();
};

size_t state_to_index(
    const int16_t states[NUM_STATES],
    const int16_t state
);
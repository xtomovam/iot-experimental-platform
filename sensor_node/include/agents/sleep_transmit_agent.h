#include "agent.h"

class SleepTransmitAgent : public Agent {
public:
    SleepTransmitAgent(
        GoalOrientedTensor *got,
        float got_weight,
        float energy_weight,
        uint8_t threshold,
        bool consider_delay,
        bool state_aware
    );

    bool antenna_action(const EnvState env_state[NUM_DIMS]) override;
    uint8_t sleep_action(const EnvState env_state[NUM_DIMS]) override;
    void update_after_sensing_step(const EnvState env_state[NUM_DIMS]) override;
    void update_after_antenna_step(const EnvState env_state[NUM_DIMS], const int feedback, const time_t transmission_time_ms) override;
    void update_after_sleep_step(const EnvState env_state[NUM_DIMS]) override;
    void reset(GoalOrientedTensor *got) override;

    void save_to_nvs(const nvs_handle_t nvs_handle) override;
    void load_from_nvs(const nvs_handle_t nvs_handle) override;

private:
    static bool already_initialized;
    GoalOrientedTensor *got;
    uint8_t base_threshold;
    static float deep_sleep_duration;
    bool state_aware;
};
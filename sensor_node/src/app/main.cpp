#include "app/main.h"

// data needed to be remembered across deep sleep cycles
RTC_DATA_ATTR uint32_t seq = 0; // current sequence number for packets
RTC_DATA_ATTR uint32_t curr_ts = -1; // current time step
RTC_DATA_ATTR uint32_t ts_sensed = 0; // time step when the last sensing was performed
RTC_DATA_ATTR EnvState env_state[NUM_DIMS]; // current environment state
RTC_DATA_ATTR time_t max_saving_time = AGENT_SAVING_MS[static_cast<size_t>(AGENT_TYPE)]; // maximum time taken to save agent data to NVS

GoalOrientedTensor *got; 
Agent *agent; 
bool antenna_action; 
uint64_t transmission_start; 
bool slept = true; 
uint64_t ts_start = 0; 

void timestep_end();

// initialize the agent based on AGENT_TYPE
void initialize_agent(bool first_time_init)  {
  if (AGENT_TYPE == AgentType::BASELINE) {
    agent = new BaselineAgent(BaselineAgentType::ALWAYS_SEND);
  } else if (AGENT_TYPE == AgentType::RANDOM) {
    agent = new RandomAgent();
  } else if (AGENT_TYPE == AgentType::SLEEP_TRANSMIT) {
    agent = new SleepTransmitAgent(
      got, // goal-oriented tensor
      1.0, // goal weight
      1.0, // energy weight
      UINT8_MAX, // threshold (UINT8_MAX -> use default)
      false, // consider delay
      false // state aware
    );
  } else if (AGENT_TYPE == AgentType::Q_LEARNING) {
    agent = new QLearningAgent(
      got, // goal-oriented tensor
      1.0, // got weight
      1.0, // energy weight
      1000, // total steps
      0.05, // learning rate
      0.9, // discount factor
      1.0, // initial epsilon
      0.01, // final epsilon
      false, // realistic states
      false // simplified states
    );
  } else if (AGENT_TYPE == AgentType::PSBO) {
    static PSBOAgent a = PSBOAgent(
      got, // goal-oriented tensor
      1.0, // got weight
      1.0, // energy weight
      first_time_init, // whether this is the first time initialization
      false // verbose
    );
    agent = &a;
  } else {
    throw std::runtime_error("Unknown AGENT_TYPE");
  }
}

float get_GoT_cost() {
  // create process_state and receiver_state arrays
  int16_t process_state[NUM_DIMS];
  int16_t receiver_state[NUM_DIMS];
  for (size_t dim = 0; dim < NUM_DIMS; dim++) {
    process_state[dim] = env_state[dim].X_proc;
    receiver_state[dim] = env_state[dim].X_rx;
  }

  // compute GoT cost
  return got->get_cost(
    process_state,
    receiver_state,
    env_state[0].AoI_rx, // suppose all dimensions have the same AoI_rx
    env_state[0].AoII // suppose all dimensions have the same AoII
  );
}

// put the device to deep sleep for sleep_ts time steps
void sleep(const uint8_t sleep_ts) {
  curr_ts += sleep_ts;
  esp_sleep_enable_timer_wakeup((sleep_ts * TIME_STEP_MS + TIME_STEP_MS - millis() % TIME_STEP_MS) * 1000);
  esp_deep_sleep_start();
}

// compute energy cost in mJ for the current time step
float get_energy_cost(const time_t time_step_end, const bool going_to_sleep) {
  time_t ms_left_until_now = millis() - ts_start;
  time_t ms_from_now = TIME_STEP_MS - ms_left_until_now;
  float energy_consumed_mJ = 0.0;

  // wake up
  if (slept) {
    energy_consumed_mJ += WAKE_UP_INTERVAL_MS * WAKE_UP_MJ_PER_MS;
  }

  // sensing
  ms_left_until_now -= SENSING_INTERVAL_MS;
  energy_consumed_mJ += SENSING_MJ_PER_MS * SENSING_INTERVAL_MS;

  // antenna
  if (antenna_action) {
    ms_left_until_now -= (time_step_end - transmission_start);
    energy_consumed_mJ += (time_step_end - transmission_start) * ANTENNA_MJ_PER_MS;
  }

  // idle up to now
  energy_consumed_mJ += ms_left_until_now * IDLE_MJ_PER_MS;

  // action from now
  if (going_to_sleep) {
    energy_consumed_mJ += ms_from_now * DEEP_SLEEP_MJ_PER_MS;
  } else {
    energy_consumed_mJ += ms_from_now * IDLE_MJ_PER_MS;
  }
  
  return energy_consumed_mJ;
}

// actions to perform at the start of each time step
void timestep_start() {
  // perform necessary updates
  ts_start = millis();
  curr_ts++;
  feedback_received = false;
  transmissions = 0;
  for (size_t dim = 0; dim < NUM_DIMS; dim++) {
    // update AoI_rx
    env_state[dim].AoI_rx = env_state[dim].AoI_rx < UINT8_MAX ? env_state[dim].AoI_rx + 1 : UINT8_MAX;
    // update AoII
    if (env_state[dim].X_rx != env_state[dim].X_tx) {
      env_state[dim].AoII = env_state[dim].AoII < UINT8_MAX ? env_state[dim].AoII + 1 : UINT8_MAX;
    } else {
      env_state[dim].AoII = 0;
    }
  }

  // sense / read from serial
  sense();

  // decide whether to transmit or not
  antenna_action = agent->antenna_action(env_state);

  // agent decides to transmit -> transmit
  if (antenna_action) {
    prepare_packet();
    initialize_BLE();
    transmission_start = millis();
    start_ble_adv();

  // agent decides to not transmit -> enter deep-sleep or wait for the next time step
  } else {
    timestep_end();
  }  
}

// actions to perform at the end of each time step
void timestep_end() {
  uint64_t time_step_end = millis();

  // update agent after transmission
  if (antenna_action) {
    agent->update_after_antenna_step(env_state, feedback_received ? transmissions : -1, time_step_end - transmission_start );
  }

  // decide sleep duration
  uint8_t sleep_ts = agent->sleep_action(env_state);

  // save agent data to NVS (if AGENT_TYPE requires it)
  time_t saving_time = 0;
  if (sleep_ts > 0) {
    uint64_t saving_start = millis();
    save_agent(agent);
    uint64_t saving_end = millis();
    saving_time = saving_end - saving_start;
    max_saving_time = saving_time > max_saving_time ? saving_time : max_saving_time;
    max_saving_time = curr_ts == 0 ? AGENT_SAVING_MS[static_cast<size_t>(AGENT_TYPE)] : max_saving_time;
  }

  // send report via serial link
  float energy_cost = get_energy_cost(time_step_end, sleep_ts > 0);
  Serial.printf("%d %f %lu\n", feedback_received, energy_cost, feedback_received ? time_step_end - transmission_start : 0);

  // update omniscient Q-learning agent
  if (AGENT_TYPE == AgentType::Q_LEARNING && static_cast<QLearningAgent *>(agent)->is_omniscient()) {
    float got_cost = get_GoT_cost();
    static_cast<QLearningAgent *>(agent)->omniscient_update(
      env_state, 
      sleep_ts,
      energy_cost + got_cost
    );
  }

  // sleep or wait for the next time step
  if (sleep_ts > 0) {
    sleep(sleep_ts); // sleep for sleep_ts time steps
  } 
  if (millis() / TIME_STEP_MS == ts_start / TIME_STEP_MS) {
    delay(pdMS_TO_TICKS(TIME_STEP_MS - millis() % TIME_STEP_MS)); // wait until the beginning of the next time step
  }
  slept = false;
  timestep_start();
}

// actions to perform on the first boot
void first_boot() {
    // initialize NVS
    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }
    
    // initialize possible states
    int16_t possible_states[NUM_DIMS][NUM_STATES];
    for (uint16_t dim = 0; dim < NUM_DIMS; dim++) {
      for (uint16_t state = 0; state < NUM_STATES; state++) {
        possible_states[dim][state] = state;
      }
    }
    
    // initialize environment state
    for (size_t dim = 0; dim < NUM_DIMS; dim++) {
      env_state[dim].X_proc = possible_states[dim][0];
      env_state[dim].X_tx = possible_states[dim][0];
      env_state[dim].X_rx = possible_states[dim][NUM_STATES - 1];
      env_state[dim].AoI_tx = UINT8_MAX;
      env_state[dim].AoI_rx = UINT8_MAX;
      env_state[dim].AoII = UINT8_MAX;
    }
}

void setup() {
  Serial.begin(115200); // initialize serial
  
  // initialize goal-oriented tensor
  got = new GoalOrientedTensor(
    GOT_METRIC,
    STATES, // possible states
    BASE_VALUES,
    GROWTHS,
    -1.0f
  ); 

  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
    first_boot(); // first boot actions
    initialize_agent(true); // initialize agent
  } else {
    initialize_agent(false); // initialize agent
    load_agent(agent); // load agent data from NVS
  }

  timestep_start(); // perform actions for the current time step
}

void loop() {}
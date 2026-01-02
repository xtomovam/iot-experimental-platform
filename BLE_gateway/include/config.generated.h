// Auto-generated config
#pragma once
#include <stdint.h>

constexpr char SENSOR_NODE_PORT[] = "/dev/ttyUSB0";
constexpr char SERVER_IP[] = "95.169.201.66";
constexpr int SERVER_PORT = 5000;
constexpr char EXPERIMENT_NAME[] = "random";
constexpr char AGENT_TYPE[] = "RANDOM";
constexpr char METRIC[] = "general";
constexpr bool READ_FROM_SERIAL = false;
constexpr uint16_t TIME_STEP_MS = 1000;
constexpr uint16_t NUM_DIMS = 1;
constexpr char[] DIM_NAMES[1] = {"light"};
constexpr uint16_t NUM_STATE = 7;
constexpr uint16_t LIGHT_BUCKETS_MAX[7] = {585, 1170, 1755, 2340, 2925, 3510, 4095};
constexpr uint16_t MAX_AOI = 60;
constexpr uint16_t ADV_INTERVAL_MS = 32;
constexpr uint16_t ADV_DURATION_MS = 96;
constexpr uint8_t SENSOR_NODE_ID = 1;
constexpr uint16_t MAX_TRANSMISSIONS = 1;
constexpr uint16_t AOI_BUCKETS_MAX[4] = {2, 8, 32, 60};
constexpr float SENSING_MJ_PER_MS = 0.2062;
constexpr float WAKE_UP_MJ_PER_MS = 0.19125;
// Auto-generated config
#pragma once
#include <stdint.h>

constexpr const char* SENSOR_NODE_PORT = "/dev/ttyUSB0";
constexpr const char* SERVER_IP = "95.169.201.66";
constexpr int SERVER_PORT = 5000;
constexpr const char* EXPERIMENT_NAME = "psbo/mid-1";
constexpr const char* AGENT_TYPE_STR = "PSBO";
constexpr const char* GOT_METRIC = "general";
constexpr bool READ_FROM_SERIAL = true;
constexpr uint16_t TIME_STEP_MS = 1000;
constexpr uint16_t NUM_DIMS = 1;
constexpr const char* DIM_NAMES[1] = { "light" };
constexpr uint16_t NUM_STATES = 7;
constexpr int16_t STATES[1][7] = { { 0, 1, 2, 3, 4, 5, 6 } };
constexpr int NUM_AOI_BUCKETS = 7;
constexpr uint16_t LIGHT_BUCKETS_MAX[7] = { 585, 1170, 1755, 2340, 2925, 3510, 4095 };
constexpr uint16_t MAX_AOI = 60;
constexpr uint16_t ADV_INTERVAL_MS = 32;
constexpr uint16_t ADV_DURATION_MS = 96;
constexpr uint8_t SENSOR_NODE_ID = 1;
constexpr const char* SENSOR_NODE_NAME = "sensor_node_1";
constexpr const char* GW_NAME = "BLE_gateway";
constexpr uint16_t MAX_TRANSMISSIONS = 1;
constexpr uint16_t AOI_BUCKETS_MAX[4] = { 2, 8, 32, 60 };
constexpr int AGENT_SAVING_MS[5] = { 0, 0, 0, 422, 100 };
constexpr double ENERGY_WEIGHT = 1.0;
constexpr double GOT_WEIGHT = 1.0;
constexpr double DEEP_SLEEP_MJ_PER_MS = 0.004867;
constexpr double WAKE_UP_MJ_PER_MS = 0.19125;
constexpr double IDLE_MJ_PER_MS = 0.13795;
constexpr double SENSING_MJ_PER_MS = 0.0;
constexpr double ANTENNA_MJ_PER_MS = 0.50755;
constexpr int SENSING_INTERVAL_MS = 0;
constexpr int WAKE_UP_INTERVAL_MS = 69;
constexpr double BASE_VALUES[1][7][7] = { { { 0, 507, 637, 352, 516, 487, 340 }, { 314, 0, 644, 308, 428, 362, 380 }, { 462, 678, 0, 340, 434, 627, 332 }, { 361, 592, 471, 0, 334, 400, 556 }, { 517, 697, 582, 506, 0, 341, 478 }, { 651, 540, 395, 521, 536, 0, 328 }, { 312, 626, 542, 583, 469, 361, 0 } } };
constexpr double GROWTHS[1][7][7] = { { { 512, 536, 666, 579, 551, 679, 456 }, { 600, 364, 626, 643, 435, 438, 691 }, { 588, 560, 530, 327, 500, 567, 347 }, { 515, 398, 659, 513, 526, 430, 304 }, { 554, 658, 692, 314, 645, 679, 362 }, { 530, 351, 687, 530, 442, 470, 335 }, { 459, 486, 385, 365, 344, 433, 583 } } };
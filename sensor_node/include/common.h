#pragma once

#include "config.h"
#include "types.h"
#include "random.h"

#include <Arduino.h> // Core Arduino functions (Serial, timing, etc.)
#include <Adafruit_AHTX0.h> // library for AHTX0 temperature and humidity sensor
#include <esp_bt.h> // bluetooth controller initialization and configuration
#include <esp_bt_main.h> // main Bluetooth stack functions (init, enable)
#include <esp_gap_ble_api.h> // BLE GAP API for advertising, scanning, and callbacks

#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <vector>
#include <string.h>
#include <algorithm>
#include <math.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <map>
#include <string.h>

// agent types
enum class AgentType {
    BASELINE,
    RANDOM,
    SLEEP_TRANSMIT,
    Q_LEARNING,
    PSBO
};

// pre-calculated constants
constexpr size_t MAX_METRIC_NAME_LEN = 64;
constexpr uint16_t SCAN_INTERVAL_MS = TIME_STEP_MS - WAKE_UP_INTERVAL_MS - ADV_DURATION_MS;
constexpr size_t NUM_STATE_COMBINATIONS = (NUM_STATES * NUM_STATES) * (NUM_AOI_BUCKETS * (NUM_AOI_BUCKETS + 1) / 2);
constexpr uint16_t DATA_PACKET_SIZE_B = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t) + /*NUM_DIMS **/ sizeof(uint16_t); // device ID + sequence number + AoI_tx + NUM_DIMS * X_tx
constexpr uint16_t FEEDBACK_PACKET_SIZE_B = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t); // device ID + sequence number + AoI_rx
constexpr float SENSING_ENERGY_MJ = SENSING_INTERVAL_MS * SENSING_MJ_PER_MS;
constexpr float WAKE_UP_ENERGY_MJ = WAKE_UP_INTERVAL_MS * WAKE_UP_MJ_PER_MS;
constexpr float EPSILON = 1e-12;

inline const AgentType getAgentType(const char* s) {
    if (strcmp(s, "BASELINE") == 0) return AgentType::BASELINE;
    if (strcmp(s, "RANDOM") == 0) return AgentType::RANDOM;
    if (strcmp(s, "SLEEP_TRANSMIT") == 0) return AgentType::SLEEP_TRANSMIT;
    if (strcmp(s, "Q-LEARNING") == 0) return AgentType::Q_LEARNING;
    if (strcmp(s, "PSBO") == 0) return AgentType::PSBO;
    return AgentType::BASELINE;
}

const AgentType AGENT_TYPE = getAgentType(AGENT_TYPE_STR);

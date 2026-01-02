#pragma once

#include "app/main.h"

extern bool feedback_received; // flag to indicate if feedback was received

bool is_from_gateway(uint8_t *feedback_packet); // check if the given packet originates from the BLE gateway
void process_feedback(uint8_t *feedback_packet); // process the feedback packet received from the BLE gateway
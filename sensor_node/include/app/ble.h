#pragma once

#include "app/main.h"
#include "app/feedback.h"

extern uint8_t transmissions;

// functions for BLE operations
void start_ble_adv();

void prepare_packet(); // prepare the data packet to be advertised
void initialize_BLE(); // initialize the Bluetooth controller (if not already initialized)
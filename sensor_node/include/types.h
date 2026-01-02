#pragma once

#include <stdint.h>

// structure type for environment state
typedef struct {
    int16_t X_proc;
    int16_t X_tx;
    int16_t X_rx;
    uint8_t AoI_tx;
    uint8_t AoI_rx;
    uint8_t AoII;
}EnvState;
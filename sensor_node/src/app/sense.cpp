#include "app/sense.h"

constexpr uint8_t LIGHT_SENSOR_PIN = 8;

Adafruit_AHTX0 sensor; // sensor object
bool sensor_initialized = false; // flag indicating if he sensor has been initialized

// sense enviroment and update env_state (single "light" dimension hardcoded)
void sense() {
  int16_t bucket = 0;

  // get environment data

  // a) bucket from serial input
  if (READ_FROM_SERIAL) {
    while (!Serial.available()) {
      vTaskDelay(pdMS_TO_TICKS(1)); // wait for data
    }
    String value = Serial.readStringUntil('\n'); // read until newline
    if (value.length() > 0) {
      bucket = value.toInt(); // update proc state
    }
    if (curr_ts == 0) { // first reading -> synchronize
      while (millis() % TIME_STEP_MS != WAKE_UP_INTERVAL_MS + SENSING_INTERVAL_MS) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      Serial.printf("SYNCH\n"); // send sync signal
    }
    vTaskDelay(pdMS_TO_TICKS(SENSING_INTERVAL_MS)); // simulate sensing time

  // b) light sensor reading
  } else {
    uint16_t light_val = analogRead(LIGHT_SENSOR_PIN); // read light sensor
    bucket = 0;
    while (light_val > LIGHT_BUCKETS_MAX[bucket]) { // determine bucket (O(NUM_STATES), could be optimized with binary search)
      bucket++;
    }
  }

  // update environment state
  env_state[0].X_proc = bucket;
  env_state[0].X_tx = bucket;
  env_state[0].AoI_tx = 0;
  if (env_state[0].X_rx != env_state[0].X_tx) {
    env_state[0].AoII = env_state[0].AoII < UINT8_MAX ? env_state[0].AoII + 1 : UINT8_MAX;
  } else {
    env_state[0].AoII = 0;
  }
  
  ts_sensed = curr_ts; // update last time sensed
}

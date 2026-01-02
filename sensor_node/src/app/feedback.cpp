#include "app/feedback.h"

bool feedback_received = false; // flag to indicate if feedback was received

// check if the given packet originates from the BLE gateway
bool is_from_gateway(uint8_t *packet) {
  uint8_t name_len = 0;
  uint8_t *name_data = esp_ble_resolve_adv_data(packet, ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
  if (!name_data || name_len == 0 || name_len > 31) return false;
  char name[32] = {0};
  memcpy(name, name_data, name_len);
  return strcmp(name, GW_NAME) == 0;
}

// process the feedback packet received from the BLE gateway
void process_feedback(uint8_t *feedback_packet) {

  // extract feedback data
  uint8_t feedback_len = 0;
  uint8_t *feedback = esp_ble_resolve_adv_data(feedback_packet, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &feedback_len);

  if (!feedback || feedback_len != FEEDBACK_PACKET_SIZE_B) return; // invalid feedback packet

  // update environment state
  for (unsigned dim = 0; dim < NUM_DIMS; dim++) {
    uint8_t aoi_rx = feedback[sizeof(SENSOR_NODE_ID) + sizeof(seq) + dim * sizeof(env_state[dim].X_tx)]; // this should be always 0 or UINT8_MAX
    if (aoi_rx != UINT8_MAX) { // feedback comming from the server
      env_state[dim].AoI_rx = aoi_rx;
      env_state[dim].X_rx = env_state[dim].X_tx; // suppose feedback was received in the same time step as the corresponding data packet was sent
    }
  }

  feedback_received = true; // set feedback received flag
}
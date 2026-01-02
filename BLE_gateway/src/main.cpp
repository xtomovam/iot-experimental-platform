#include "config.h"

#include <ETH.h>
#include <WiFiClient.h>
#include <BLEDevice.h>
#include "esp_gap_ble_api.h"

#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

constexpr uint16_t DATA_PACKET_SIZE_B = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t) + /*NUM_DIMS **/ sizeof(uint16_t); // device ID + sequence number + AoI_tx + NUM_DIMS * X_tx
constexpr uint16_t FEEDBACK_PACKET_SIZE_B = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t); // device ID + sequence number + AoI_rx
constexpr uint16_t SCAN_INTERVAL_MS = ADV_DURATION_MS;

// eth parameters
constexpr uint8_t ETH_ADDR = 0;
constexpr int8_t ETH_POWER_PIN = -1;
constexpr uint8_t ETH_MDC_PIN = 23;
constexpr uint8_t ETH_MDIO_PIN = 18;
constexpr eth_phy_type_t ETH_TYPE = ETH_PHY_LAN8720;
WiFiClient client;
uint8_t feedback[FEEDBACK_PACKET_SIZE_B] = {0};
bool sent_to_server = false;
time_t last_send_time = 0;

// BLE parameters
esp_ble_adv_data_t adv_data = {
  .include_name = true,
  .manufacturer_len = FEEDBACK_PACKET_SIZE_B,
  .p_manufacturer_data = feedback,
};
esp_ble_adv_params_t adv_params = {
  .adv_int_min = ADV_INTERVAL_MS,
  .adv_int_max = ADV_INTERVAL_MS,
  .adv_type = ADV_TYPE_NONCONN_IND,
  .channel_map = ADV_CHNL_ALL,
};
esp_ble_scan_params_t scan_params = {
  .scan_type = BLE_SCAN_TYPE_PASSIVE,
  .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval = SCAN_INTERVAL_MS,
  .scan_window = SCAN_INTERVAL_MS,
};

// timer for advertising duration
TimerHandle_t adv_timer = xTimerCreate(
  "AdvTimer", pdMS_TO_TICKS(ADV_DURATION_MS), pdFALSE, NULL, [](TimerHandle_t xTimer) {
    esp_ble_gap_stop_advertising();
  }
);

bool send_to_server(const uint8_t* packet, const size_t size) {
    // check connection
    if (!client.connected()) {
        client.stop(); // make sure socket is clean
        if (!client.connect(SERVER_IP, SERVER_PORT)) {
            Serial.println("Failed to connect to server!");
            return false;
        }
        Serial.println("Connected to server.");
    }

    // send packet
    int written = client.write(packet, size);
    if (written != size) {
        Serial.println("Failed to send full packet");
        client.stop();
        return false;
    }

    // wait for response
    uint64_t start = millis();
    while (!client.available()) {
        if (millis() - start > TIME_STEP_MS - ADV_DURATION_MS) { // timeout after TIME_STEP_MS - ADV_DURATION_MS ms
            Serial.printf("[%lu] Timeout waiting for response\n", millis());
            return false;
        }
    }

    // read response
    size_t len = client.read(feedback, FEEDBACK_PACKET_SIZE_B);
    Serial.printf("[%lu] Got %d bytes feedback\n", millis(), (int)len);

    return (len > 0);
}

bool is_from_sensor_node(uint8_t *data_packet) {
  char sensor_node_name[32] = {0};
  memcpy(sensor_node_name, SENSOR_NODE_NAME, strlen(SENSOR_NODE_NAME));
  uint8_t adv_name_len = 0;
  const uint8_t *adv_name = esp_ble_resolve_adv_data(data_packet, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
  if (!adv_name || adv_name_len == 0 || adv_name_len > 31) return false;
  char device_name[32] = {0};
  memcpy(device_name, adv_name, adv_name_len);
  if (strcmp(device_name, SENSOR_NODE_NAME) != 0) return false;
  return true;
}

// callback function for BLE GAP events
void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // case for BLE scan results
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
        if (!is_from_sensor_node(param->scan_rst.ble_adv) || sent_to_server) break; // ignore if not from sensor node oralready processed
        Serial.printf("[%lu] Received BLE advertisement from sensor node\n", millis());
        // send the received packet to the server
        sent_to_server = true;
        esp_ble_gap_stop_scanning();
        if (send_to_server(param->scan_rst.ble_adv, param->scan_rst.adv_data_len)) {
          esp_ble_gap_start_advertising(&adv_params); // if feedback from the server is received, transmit it via BLE
        } else {
          esp_ble_gap_start_scanning(0); // resume scanning if no feedback is received
        }
      }
      break;
    }

    // stopped scanning -> start advertising for ADV_DURATION_MS ms
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      xTimerStart(adv_timer, 0);
      break;
    }

    // stopped advertising -> start scanning
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      esp_ble_gap_start_scanning(0);
      break;
    }

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      while (millis() - last_send_time < TIME_STEP_MS - ADV_DURATION_MS) {
        vTaskDelay(pdMS_TO_TICKS(1)); // wait until TIME_STEP_MS - ADV_DURATION_MS ms passed since last send
      }
      sent_to_server = false; // reset flag to allow processing new packets
      break;
    }

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Gateway starting...");
  
  // eth initialization
  if (!ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE)) {
    exit(EXIT_FAILURE);
  }

  // BLE initialization
  BLEDevice::init(GW_NAME);
  esp_ble_gap_register_callback(gap_callback);
  esp_ble_gap_set_device_name(GW_NAME);
  esp_ble_gap_config_adv_data(&adv_data);
  esp_ble_gap_set_scan_params(&scan_params);

  // start scanning
  Serial.println("Starting BLE scan...");
  esp_ble_gap_start_scanning(0);
}

void loop() {}
#include "app/ble.h"

uint8_t transmissions = 0; // number of transmissions in the current time step

bool BLE_initialized = false; // flag to indicate if BLE is initialized
uint8_t packet_data[DATA_PACKET_SIZE_B] = {0}; // buffer for data packet to be sent

// tasks and queues
QueueHandle_t timestep_queue;
EventGroupHandle_t ble_events;
constexpr uint32_t EVT_START_ADV = (1 << 0);
constexpr uint32_t EVT_STOP_ADV = (1 << 1);
constexpr uint32_t EVT_START_SCAN = (1 << 2);
constexpr uint32_t EVT_STOP_SCAN = (1 << 3);
constexpr uint32_t EVT_TIMESTEP_END  = (1 << 4);

// BLE parameters
esp_ble_scan_params_t scan_params = {
  .scan_type = BLE_SCAN_TYPE_PASSIVE,
  .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval = SCAN_INTERVAL_MS,
  .scan_window = SCAN_INTERVAL_MS,
};
esp_ble_adv_params_t adv_params = {
  .adv_int_min = ADV_INTERVAL_MS,
  .adv_int_max = ADV_INTERVAL_MS,
  .adv_type = ADV_TYPE_NONCONN_IND,
  .channel_map = ADV_CHNL_ALL,
};
esp_ble_adv_data_t adv_data = {
  .include_name = true,
  .manufacturer_len = DATA_PACKET_SIZE_B,
  .p_manufacturer_data = packet_data,
};

// timers for BLE operations
void stop_ble_adv();
TimerHandle_t adv_timer = xTimerCreate(
  "AdvTimer", pdMS_TO_TICKS(ADV_DURATION_MS), pdFALSE, NULL, [](TimerHandle_t xTimer) {
    stop_ble_adv();
  }
);
TimerHandle_t scan_timer = NULL; // created when starting scan

// functions for BLE operations
void start_ble_adv() {
  xEventGroupSetBits(ble_events, EVT_START_ADV);
}
void stop_ble_adv() {
  xEventGroupSetBits(ble_events, EVT_STOP_ADV);
}
void start_ble_scan() {
  xEventGroupSetBits(ble_events, EVT_START_SCAN);
}
void stop_ble_scan() {
  xEventGroupSetBits(ble_events, EVT_STOP_SCAN);
}
void jump_to_timestep_end() {
  xEventGroupSetBits(ble_events, EVT_TIMESTEP_END);
}

// BLE task for handling BLE events
void ble_task(void *arg) {
  for (;;) {
    EventBits_t bits = xEventGroupWaitBits(
      ble_events,
      EVT_START_ADV | EVT_STOP_ADV |
      EVT_START_SCAN | EVT_STOP_SCAN |
      EVT_TIMESTEP_END,
      pdTRUE,
      pdFALSE,
      portMAX_DELAY
    );

    if (bits & EVT_START_ADV) {
      esp_ble_gap_start_advertising(&adv_params);
    }
    if (bits & EVT_STOP_ADV) {
      esp_ble_gap_stop_advertising();
    }
    if (bits & EVT_START_SCAN) {
      esp_ble_gap_start_scanning(0);
    }
    if (bits & EVT_STOP_SCAN) {
      esp_ble_gap_stop_scanning();
    }
    if (bits & EVT_TIMESTEP_END) {
      uint8_t msg = 1;
      xQueueSend(timestep_queue, &msg, 0);
    }
  }
}

// worker task for processing end of time step
void worker_task(void *arg) {
  for (;;) {
    uint8_t msg;
    xQueueReceive(timestep_queue, &msg, portMAX_DELAY);

    timestep_end();
  }
}

// prepare the data packet to be advertised
void prepare_packet() {
  size_t size = 0;

  // device ID
  packet_data[size] = SENSOR_NODE_ID;
  size += sizeof(SENSOR_NODE_ID);

  // sequence number
  memcpy(packet_data + size, &seq, sizeof(seq));
  size += sizeof(seq);
  seq++;

  // AoI (should be always 0)
  for (size_t dim = 0; dim < NUM_DIMS; dim++) {
    memcpy(packet_data + size, &env_state[dim].AoI_tx, sizeof(env_state[dim].AoI_tx));
    size += sizeof(env_state[dim].AoI_tx);
  }

  // environmental data
  for (size_t dim = 0; dim < NUM_DIMS; dim++) {
    memcpy(packet_data + size, &env_state[dim].X_tx, sizeof(env_state[dim].X_tx));
    size += sizeof(env_state[dim].X_tx);
  }
}

// callback function for BLE GAP events
void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {

    // case for BLE scan results
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
        if (!is_from_gateway(param->scan_rst.ble_adv)) break; // ignore if not from gateway

        // process feedback packet
        process_feedback(param->scan_rst.ble_adv);

        // stop scanning
        xTimerStop(scan_timer, 0);
        stop_ble_scan();
      }
      break;
    }

    // started advertising -> set timer to stop advertising after ADV_DURATION_MS milliseconds
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      xTimerStart(adv_timer, 0);
      transmissions++; // increment transmission count
      break;
    }

    // stopped advertising -> start scanning
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      start_ble_scan();
      break;
    }

    // started scanning -> set timer to stop scanning at the end of the time current step
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      if (scan_timer) xTimerDelete(scan_timer, 0);
      time_t ms_left = TIME_STEP_MS - millis() % TIME_STEP_MS;
      time_t time_for_scanning = ms_left - max_saving_time - 5;
      if (time_for_scanning <= ADV_DURATION_MS) {
        max_saving_time -= ADV_DURATION_MS;
        stop_ble_scan();
        break;
      }
      scan_timer = xTimerCreate(
        "ScanTimer", pdMS_TO_TICKS(time_for_scanning), pdFALSE, NULL, [](TimerHandle_t xTimer) {
          stop_ble_scan();
        }
      );
      xTimerStart(scan_timer, 0);
      break;
    }

    // stopped scanning -> proceed to the end of the time step
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
      jump_to_timestep_end();
      break;
    }

    default:
      break;
  }
}

// initialize the Bluetooth controller (if not already initialized)
void initialize_BLE() {
  if (BLE_initialized) return;
  
  // initialize Bluetooth controller
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  esp_bt_controller_enable(ESP_BT_MODE_BLE);
  
  // initialize Bluedroid stack
  esp_bluedroid_init();
  esp_bluedroid_enable();
  
  // register GAP callback
  esp_ble_gap_register_callback(gap_callback);
  
  // set GAP parameters
  esp_ble_gap_set_device_name(SENSOR_NODE_NAME);
  esp_ble_gap_config_adv_data(&adv_data);
  esp_ble_gap_set_scan_params(&scan_params);
  
  // create tasks
  ble_events = xEventGroupCreate();
  xTaskCreatePinnedToCore(ble_task, "ble_task", 16384, nullptr, 1, nullptr, 0);

  timestep_queue = xQueueCreate(4, sizeof(uint8_t));
  xTaskCreatePinnedToCore(worker_task, "worker_task", 32768, nullptr, 1, nullptr, 1);

  
  BLE_initialized = true;
}
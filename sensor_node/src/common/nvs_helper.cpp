#include "nvs_helper.h"

// Initialize NVS using the specified partition
nvs_handle_t initialize_nvs(std::string partition_label, std::string name) {
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, partition_label.c_str());
    if (!part) {
        exit(EXIT_FAILURE);
    }
    esp_err_t err = nvs_flash_init_partition(part->label);
    if (err != ESP_OK && err != ESP_ERR_NVS_NO_FREE_PAGES && err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        exit(EXIT_FAILURE);
    }

    nvs_handle_t nvs_handle;
    err = nvs_open_from_partition(partition_label.c_str(), name.c_str(), NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        exit(EXIT_FAILURE);
    }

    return nvs_handle;
}

void save_agent(Agent *agent) {
    nvs_handle_t nvs_handle = initialize_nvs("nvs2", "agent_storage");
    agent->save_to_nvs(nvs_handle);
}

void load_agent(Agent *agent) {
    nvs_handle_t nvs_handle = initialize_nvs("nvs2", "agent_storage");
    agent->load_from_nvs(nvs_handle);
}
#include "random.h"

// generate a random float in [min, max)
float custom_random(const float &min, const float &max) {
    uint32_t r = esp_random();
    float normalized = (float)r / ((float)UINT32_MAX + 1.0f); // 0.0 <= x < 1.0
    return min + normalized * (max - min);
}

// generate a random uint8_t in [min, max)
uint8_t custom_random_uint8(const uint8_t &min, const uint8_t &max) {
    uint32_t r = esp_random();
    float normalized = (float)r / ((float)UINT32_MAX + 1.0f); // 0.0 <= x < 1.0
    return min + (uint8_t)(normalized * (float)(max - min));
}

uint8_t random_from_aoi_bucket(const size_t &bucket_index) {
    uint8_t min = bucket_index == 0 ? 0 : AOI_BUCKETS_MAX[bucket_index - 1] + 1;
    uint8_t max = AOI_BUCKETS_MAX[bucket_index];
    return custom_random_uint8(min, max + 1); // random within the bucket
}

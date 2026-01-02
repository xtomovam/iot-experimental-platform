#include "config.h"

#include <esp_random.h>

float custom_random(const float &min, const float &max);
uint8_t custom_random_uint8(const uint8_t &min, const uint8_t &max);
uint8_t random_from_aoi_bucket(const size_t &bucket_index);
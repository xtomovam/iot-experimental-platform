#include "aos_table.h"


void AoSTable::copy(const AoSTable &src, AoSTable &dest) {
    dest.clear();
    
    for (const auto &pair : src.get_table()) {
        dest.add_or_update(pair.first, pair.second);
    }
}

void AoSTable::add_or_update(const Key &key, double value) {
    this->table[key] = value;
}

bool AoSTable::get(const Key &key, double &out_value) const {
    auto it = this->table.find(key);
    if (it != this->table.end()) {
        out_value = it->second;
        return true;
    }
    return false;
}

void AoSTable::clear() {
    std::map<Key, double>().swap(this->table);
}

uint8_t *AoSTable::serialize(size_t &out_len) const {
    // calculate output length
    size_t count = this->table.size();
    out_len = count * (sizeof(Key) + sizeof(double));
    if (out_len == 0) {
        return NULL;
    }

    // allocate buffer
    uint8_t *buffer = (uint8_t *)malloc(out_len);
    if (!buffer) {
        throw std::runtime_error("Malloc failed in serialize()");
    }

    // serialize entries
    uint8_t *ptr = buffer;
    size_t idx = 0;
    for (auto it = this->table.begin(); it != this->table.end(); ++it, ++idx) {
        memcpy(ptr, &it->first, sizeof(Key));
        ptr += sizeof(Key);
        memcpy(ptr, &it->second, sizeof(double));
        ptr += sizeof(double);
    }

    return buffer;
}

void AoSTable::deserialize(const uint8_t *buffer, const size_t len) {
    // clear existing table
    this->clear();
    
    // deserialize entries
    size_t offset = 0;
    while (offset + sizeof(Key) + sizeof(double) <= len) {
        Key key;
        double value;
        memcpy(&key, buffer + offset, sizeof(Key));
        offset += sizeof(Key);
        memcpy(&value, buffer + offset, sizeof(double));
        offset += sizeof(double);

        this->add_or_update(key, value);
    }
}

bool AoSTable::is_empty() const {
    return this->table.empty();
}

const std::map<Key, double> &AoSTable::get_table() const {
    return this->table;
}

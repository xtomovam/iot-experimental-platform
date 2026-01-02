#pragma once

#include "common.h"

struct Key {
    int16_t X_proc;
    int16_t X_rx;
    uint8_t AoII;
    uint8_t padding;

    bool operator<(const Key &other) const noexcept {
        if (X_proc != other.X_proc) return X_proc < other.X_proc;
        if (X_rx   != other.X_rx)   return X_rx   < other.X_rx;
        return AoII < other.AoII;
    }

    bool operator==(const Key &other) const noexcept {
        return X_proc == other.X_proc &&
               X_rx   == other.X_rx &&
               AoII == other.AoII;
    }
};

class AoSTable {
public:
    static void copy(const AoSTable &src, AoSTable &dest);

    void add_or_update(const Key &key, double value);
    bool get(const Key &key, double &out_value) const;
    void clear();
    bool is_empty() const;

    uint8_t *serialize(size_t &out_size) const;
    void deserialize(const uint8_t *data, const size_t size);

    const std::map<Key, double> &get_table() const;

private:
    std::map<Key, double> table;
};
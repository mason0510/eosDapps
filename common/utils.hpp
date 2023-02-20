#pragma once

#include <string>

namespace godapp {
    using namespace std;

    string symbol_to_string(uint64_t v) {
        v >>= 8;
        string result;
        while (v > 0) {
            char c = static_cast<char>(v & 0xFF);
            result += c;
            v >>= 8;
        }
        return result;
    }
}

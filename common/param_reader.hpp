#pragma once

#include <string>
#include <eosiolib/eosio.hpp>
#include "./constants.hpp"

namespace godapp {
    using namespace std;
    using namespace eosio;

    class param_reader {
    public:
        param_reader(const string& params, char separator = ','):_params(params), _last_pos(0) {
        }

        string next_param(const char* error_msg = "param missing") {
            size_t new_pos = _params.find(',', _last_pos);
            eosio_assert(new_pos >= 0, error_msg);

            string result = _params.substr(_last_pos, new_pos - _last_pos);
            _last_pos = new_pos + 1;
            return result;
        }

        uint8_t next_param_i(const char* error_msg = "param missing") {
            return (uint8_t) atoi(next_param(error_msg).c_str());
        }

        uint64_t next_param_i64(const char* error_msg = "param missing") {
            return (uint64_t) atoi(next_param(error_msg).c_str());
        }

        string rest() {
            return _params.substr(_last_pos, _params.length() - _last_pos);
        }

        bool has_next() {
            return _last_pos < _params.length();
        }

        name get_referer(name from, name default_referer = HOUSE_ACCOUNT) {
            string referer_name = next_param("referrer is missing");
            if (referer_name.empty()) {
                return default_referer;
            } else {
                name referer(referer_name);
                if (from.value == referer.value) {
                    return default_referer;
                }

                return is_account(referer) ? referer : default_referer;
            }
        }

    private:
        const string& _params;
        size_t _last_pos;
    };
}
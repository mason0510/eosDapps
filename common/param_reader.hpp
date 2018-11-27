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
            eosio_assert(new_pos > 0, error_msg);

            string result = _params.substr(_last_pos, new_pos - _last_pos);
            _last_pos = new_pos + 1;
            return result;
        }

        name get_referrer(name from, name default_referrer = TEAM_ACCOUNT) {
            string referrer_name = next_param("referrer is missing");
            if (referrer_name.empty()) {
                return default_referrer;
            } else {
                name referrer = name(referrer_name);
                if (from == referrer) {
                    return default_referrer;
                }

                eosio_assert(is_account(referrer), "referrer does not exist");
                return referrer;
            }
        }

    private:
        const string& _params;
        size_t _last_pos;
    };
}
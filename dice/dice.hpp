#pragma once

#include <eosiolib/eosio.hpp>
#include <string>
#include <vector>

#include "../common/constants.hpp"
#include "../common/random.hpp"
#include "../common/utils.hpp"
#include "../common/contracts.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT dice: public contract {
    public:
        using contract::contract;

        DEFINE_GLOBAL_TABLE

        TABLE bet {
            uint64_t id;
            uint64_t bet_id;
            symbol sym;
            name player;
            name referer;
            uint64_t bet;
            vector<asset> payout;
            uint8_t roll_type;
            uint64_t bet_value;
            uint64_t roll_value;
            capi_checksum256 seed;
            time_point_sec time;

            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("bets"), bet> bet_index;

        ACTION init();
        ACTION play(name player, asset bet, string memo);
        ACTION resolve(name player, asset bet_asset, uint8_t roll_type, uint8_t bet_number, name referrer);

        ACTION receipt(uint64_t bet_id, name player, asset bet, vector<asset> payout_list,
                       capi_checksum256 seed, uint8_t roll_type, uint64_t bet_value, uint64_t roll_value);

        dice(name receiver, name code, datastream<const char *> ds) :
        contract(receiver, code, ds),
        _globals(_self, _self.value) {
        }
    };

    EOSIO_ABI_EX(dice, (init)(play)(resolve)(receipt))
}

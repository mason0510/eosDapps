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
            name player;
            symbol sym;
            uint64_t bet;
            uint64_t payout;
            uint64_t bet_value;
            uint64_t roll_value;
            time_point_sec time;

            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("games"), bet> bet_index;

        ACTION init();
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION play(uint64_t bet_id, name player, asset bet_asset, uint8_t bet_number, name referrer);
        ACTION resolve(uint64_t bet_id, name player, asset bet_asset, uint8_t bet_number, name referrer);

        ACTION pay(uint64_t bet_id, name player, asset bet, asset payout, capi_checksum256 seed,
                uint8_t bet_value, uint64_t roll_value, name referer);
        ACTION transfer(name from, name to, asset quantity, string memo);

        dice(name receiver, name code, datastream<const char *> ds) :
        contract(receiver, code, ds),
        _globals(_self, _self.value) {
        }
    };

    EOSIO_ABI_EX(dice, (init)(transfer)(resolve)(play)(pay))
}

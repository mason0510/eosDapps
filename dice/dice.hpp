#pragma once

#include <eosiolib/eosio.hpp>
#include <string>
#include <vector>

#include "../common/constants.hpp"
#include "../common/random.hpp"
#include "../common/utils.hpp"
#include "../common/game_contracts.hpp"
#include "../common/contracts.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT dice: public contract {
    public:
        using contract::contract;

        DEFINE_GLOBAL_TABLE
        DEFINE_RANDOM_KEY_TABLE

        TABLE active_bet {
            uint64_t id;
            name player;
            name referer;
            uint8_t bet_number;
            asset bet_asset;
            capi_checksum256 seed;
            time_point_sec time;

            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("activebets"), active_bet> active_bet_index;
        active_bet_index _active_bets;

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
        ACTION reveal(uint64_t bet_id, capi_signature sig);
        ACTION pay(uint64_t bet_id, name player, asset bet, asset payout, capi_checksum256 seed,
                uint8_t bet_value, uint64_t roll_value, name referer);
        ACTION transfer(name from, name to, asset quantity, string memo);

        dice(name receiver, name code, datastream<const char *> ds);
    };

    EOSIO_ABI_EX(dice, (init)(transfer)(reveal)(pay))
}

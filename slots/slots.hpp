#pragma once

#include <eosiolib/eosio.hpp>
#include <string>

#include "../common/constants.hpp"
#include "../common/random.hpp"
#include "../common/utils.hpp"
#include "../common/game_contracts.hpp"
#include "../common/contracts.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT slots: public contract {
    public:
        using contract::contract;

        DEFINE_GLOBAL_TABLE
        DEFINE_RANDOM_KEY_TABLE

        TABLE active_game {
            uint64_t    id;
            name        player;
            name        referer;
            asset       price;
            uint16_t    result;

            capi_checksum256 seed;
            time_point_sec time;

            uint64_t primary_key() const { return player.value; };
            uint64_t byid()const {return id;}
        };
        typedef eosio::multi_index<name("activegame"), active_game,
            indexed_by< name("byid"), const_mem_fun<active_game, uint64_t, &active_game::byid> >
        > card_index;
        card_index _active_games;


        TABLE history {
            uint64_t id;
            uint64_t game_id;
            name player;

            capi_checksum256 seed;

            asset price;
            uint16_t result;

            time_point_sec time;

            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("history"), history> history_table;

        ACTION init();
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION reveal(uint64_t game_id, capi_signature sig);
        ACTION pay(uint64_t card_id, name player, asset price, asset reward, capi_checksum256 seed,
            uint16_t result, name referer);
        ACTION transfer(name from, name to, asset quantity, string memo);
        slots(name receiver, name code, datastream<const char *> ds);
    };

    EOSIO_ABI_EX(slots, (init)(setglobal)(transfer)(reveal)(pay))
}

/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <string>
#include "../common/contracts.hpp"

namespace godapp {
    using namespace std;
    using namespace eosio;

    class house: public contract {
    public:
        TABLE game {
            name name;
            bool active;

            uint64_t primary_key() const { return name.value; };
        };
        typedef multi_index<name("games"), game> game_index;


        TABLE token {
            symbol sym;
            name contract;

            uint64_t in = 0;
            uint64_t out = 0;
            uint64_t play_times = 0;

            uint64_t min;
            uint64_t max;
            uint64_t balance;

            uint64_t primary_key() const { return sym.raw(); };
        };
        typedef multi_index<name("tokens"), token> token_index;


        TABLE player {
            name name;

            uint64_t in;
            uint64_t out;
            uint64_t play_times;

            uint32_t last_play_time;
            uint64_t event_in;

            uint64_t primary_key() const { return name.value; };
            uint64_t byeventin()const {return event_in;}
        };
        typedef multi_index<name("player"), player,
                indexed_by< name("byeventin"), const_mem_fun<player, uint64_t, &player::byeventin> >
        > player_index;

        house(name receiver, name code, datastream<const char *> ds): contract(receiver, code, ds) {
        }

        ACTION addgame(name game);
        ACTION transfer(name from, name to, asset quantity, string memo);
        ACTION updatetoken(name game, symbol token, name contract, uint64_t min, uint64_t max, uint64_t risk_line);
    };

    EOSIO_ABI_EX(house, (transfer)(addgame)(updatetoken))
}

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

    CONTRACT scratch: public contract {
    public:
        using contract::contract;

        DEFINE_GLOBAL_TABLE
        DEFINE_RANDOM_KEY_TABLE

        TABLE active_card {
            uint64_t    id;
            name        player;
            name        referer;

            asset       price;
            asset       reward;
            uint64_t    result;

            capi_checksum256 seed;
            time_point_sec time;

            uint64_t primary_key() const { return player.value; };
            uint64_t byid()const {return id;}
        };
        typedef eosio::multi_index<name("activecard"), active_card,
            indexed_by< name("byid"), const_mem_fun<active_card, uint64_t, &active_card::byid> >
        > card_index;
        card_index _active_cards;

        TABLE history {
            uint64_t id;
            uint64_t bet_id;
            name player;

            asset price;
            asset reward;
            uint64_t    result;

            time_point_sec time;

            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("histories"), history> history_index;

        class line_result {
            uint64_t    type;
            string      result;
        };

        ACTION init();
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION reveal(uint64_t card_id, capi_signature sig);
        ACTION pay(uint64_t card_id, name player, asset price, asset reward, capi_checksum256 seed,
                line_result result[5], name referer);
        ACTION transfer(name from, name to, asset quantity, string memo);

        scratch(name receiver, name code, datastream<const char *> ds);
    };

    EOSIO_ABI_EX(scratch, (init)(setglobal)(transfer)(reveal)(pay))
}

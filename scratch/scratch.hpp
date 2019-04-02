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
            uint8_t     card_type;
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


        TABLE available_card {
            name player;
            uint32_t card1_count;
            uint32_t card2_count;
            uint32_t card3_count;
            uint32_t card4_count;
            uint64_t primary_key() const { return player.value; };
        };
        typedef eosio::multi_index<name("cards"), available_card> available_card_index;
        available_card_index _available_cards;

        TABLE history {
            uint64_t id;
            uint64_t card_id;
            name player;

            uint8_t card_type;
            capi_checksum256 seed;

            asset price;
            asset reward;
            uint64_t    result;

            time_point_sec time;

            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("history"), history> history_table;

        class line_result {
        public:
            uint64_t    type;
            string      result;
        };

        ACTION init();
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION reveal(uint64_t card_id, capi_signature sig);
        ACTION receipt(uint64_t card_id, name player, asset price, asset reward, capi_checksum256 seed,
            std::vector<line_result> result, name referer);
        ACTION transfer(name from, name to, asset quantity, string memo);
        ACTION play(name player, uint8_t card_type, name referer);
        ACTION claim(name player);
        ACTION secretsend(name player);

        scratch(name receiver, name code, datastream<const char *> ds);

    private:
        void scratch_card(name player, uint8_t card_type, asset price, name referer);
        void doClaim(name player);
    };

    EOSIO_ABI_EX(scratch, (claim)(init)(play)(reveal)(receipt)(secretsend)(setglobal)(transfer))
}

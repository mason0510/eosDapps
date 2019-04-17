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

    CONTRACT house: public contract {
    public:
        TABLE game {
            name name;
            uint64_t id;
            bool active;

            uint64_t primary_key() const { return name.value; };
            uint64_t by_id() const { return id; };
        };
        typedef multi_index<name("game"), game,
                indexed_by< name("byid"), const_mem_fun<game, uint64_t, &game::by_id> >
        > game_index;

        DEFINE_RANDOM_KEY_TABLE

        TABLE token {
            symbol sym;
            name contract;

            uint64_t in = 0;
            uint64_t out = 0;
            uint64_t play_times = 0;

            uint64_t min;
            uint64_t max_payout;
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
        typedef multi_index<name("players"), player,
                indexed_by< name("byeventin"), const_mem_fun<player, uint64_t, &player::byeventin> >
        > player_index;

        house(name receiver, name code, datastream<const char *> ds): contract(receiver, code, ds) {
        }

        ACTION addgame(name game, uint64_t id);
        ACTION updategame(name game, uint64_t id);
        ACTION setactive(name game, bool active);
        ACTION setrandkey(capi_public_key key);
        ACTION transfer(name from, name to, asset quantity, string memo);
        ACTION pay(name game, name to, asset bet, asset payout, string memo, name referer);
        ACTION updatetoken(name game, symbol token, name contract, uint64_t min, uint64_t max, uint64_t balance);
        ACTION cleartoken(name game);


        ACTION setreferer(name player, name referer);
        ACTION claimreward(name player, uint8_t reward_type);
        ACTION openchest(name player, uint8_t chest_type);
    };

#ifdef DEFINE_DISPATCHER
    EOSIO_ABI_EX(house, (transfer)(addgame)(updatetoken)(pay)(setactive)(setrandkey)(cleartoken)
    (claimreward)(setreferer)(openchest))
#endif
}

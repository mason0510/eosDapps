#pragma once
#include <eosiolib/eosio.hpp>
#include <string>
#include <vector>
#include "../common/constants.hpp"
#include "../common/random.hpp"
#include "../common/utils.hpp"
#include "../common/game_contracts.hpp"
#include "../common/contracts.hpp"
#include "../house/house.hpp"
namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT centergame: public contract {
    public:
        using contract::contract;

        DEFINE_GLOBAL_TABLE

        TABLE bet_record {
            uint64_t id;
            name player;
            uint64_t game_id;
            asset bet_asset;
            name referer;
            string self_content; 
            uint64_t time; 
            uint64_t primary_key() const { return id; };
        };
        typedef multi_index<name("bets"), bet_record> bet_record_index;
        bet_record_index _bets;

        ACTION init();
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION transfer(name from, name to, asset quantity, string memo);
        ACTION reveal(uint8_t game_id, const std::string& message, const std::vector<uint8_t>& bet_ids,
            const std::vector<asset>& prize_amounts);
        ACTION payment(uint64_t id, name player, name referer, const std::string& message, asset bet, asset payout);

        centergame(name receiver, name code, datastream<const char *> ds);
   };
  EOSIO_ABI_EX(centergame, (transfer)(setglobal)(reveal)(payment)(init))
}

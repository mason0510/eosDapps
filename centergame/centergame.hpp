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
        std::vector<uint8_t> prize_amounts;
        std::vector<uint8_t> betIds;
        std::vector<name> reward_names;

      TABLE player_record {
            uint64_t id;
            name player;
            uint64_t game_id;
            asset bet_asset;
            name referer;
            string self_content; 
            uint64_t time; 
            uint64_t primary_key() const { return id; };
        };
        typedef multi_index<name("playerrecord"), player_record> bet_record_index; bet_record_index _bets;
       
         TABLE betsprize_records {
            name player;
            uint64_t game_id;
            uint64_t prize_amount;
            uint32_t prize_time;
            uint64_t primary_key() const { return player.value; };
        };
        typedef eosio::multi_index<"prizerecords"_n, betsprize_records> prize_index;prize_index _betprizes;
    ACTION transfer(name from, name to, asset quantity, string memo);
    ACTION reveal(uint8_t game_id,std::vector<uint8_t>& betIds,std::vector<name>& reward_names,std::vector<asset>& prize_amounts);
    centergame(name receiver, name code, datastream<const char *> ds);
   };
  EOSIO_ABI_EX(centergame, (transfer)(reveal))
}

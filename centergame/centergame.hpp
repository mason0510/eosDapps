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

CONTRACT centergame : public contract {
  public:
    using contract::contract;
    string freecontent;//默认初始化，一个空字符串
    std::vector<uint8_t> prize_amounts;
    std::vector<uint8_t> betIds;
    std::vector<name> reward_names;
    std::vector<int> betamount = {1000, 2000, 10000, 20000,30000,60000};


    ACTION init();
    ACTION test(string memo);
    ACTION setglobal(uint64_t key, uint64_t value);
    ACTION transfer(name from, name to, asset quantity, string memo);
    ACTION reveal(uint8_t game_id,std::vector<uint8_t>& betIds,std::vector<name>& reward_names,std::vector<uint8_t>& prize_amounts);
   centergame(name receiver, name code, datastream<const char *> ds);
  //recharge houseaccount


  public:
    TABLE globalvar { \
        uint64_t id; \
        uint64_t val; \
        uint64_t primary_key() const { return id; }; \
    }; \
    typedef multi_index<name("globals"), globalvar> global_index; \
    global_index _globals;
        TABLE player_record {
            uint64_t id;
            name player;
            uint64_t game_id;
            uint64_t bet_amount;
            uint32_t play_time;   
            name referer;
            std::string free_content; 
            time_point_sec time; 
            uint64_t primary_key() const { return id; };
            uint64_t byreferer() const {return referer.value;}
        };
        typedef multi_index<name("playerrecord"), player_record,
            indexed_by< name("byreferer"), const_mem_fun<player_record, uint64_t, &player_record::byreferer> >
        > bet_record_index;
        bet_record_index bet_records;

  //returnback reward record
         TABLE players {
            name player;
            uint64_t game_id;
            uint64_t prize_amount;
            uint32_t prize_time;
            uint64_t primary_key() const { return player.value; };
        };
        typedef eosio::multi_index<"players"_n, players> players_index;
        players_index players;

     bool is_balance_within_range(asset quantity){
     for(int n : betamount) {
        if(n==quantity.amount){
            return true;
        }else{
          return false;
        }
    }
}  
};
EOSIO_ABI_EX(centergame,(init)(test)(transfer)(reveal));
}
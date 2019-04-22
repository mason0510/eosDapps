#include "centergame.hpp"
#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>
#include "../common/utils.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/game_contracts.hpp"

#define GLOBAL_ID_START 0
#define MIN_BETAMOUNT 1000
#define MAX_BETAmount 60000
#define BET_BEGINNUMBER 0
#define GLOBAL_ID_END 1003
#define GLOBAL_ID_BET 1001

//"0,," 游戏id推荐人 dice
 #define DEFINE_SET_GLOBAL(NAME) \
    void NAME::setglobal(uint64_t key, uint64_t value) { \
        require_auth(_self); \
        set_global(_globals, key, value); \
    }

namespace godapp {
    centergame::centergame(name receiver, name code, datastream<const char *> ds) :
    contract(receiver, code, ds),
    _globals(_self, _self.value),
    bet_records(_self, _self.value),
    players(_self, _self.value) {
    }

    void centergame::setglobal(uint64_t key, uint64_t value) { 
        require_auth(_self); 
        set_global(_globals, key, value);
    }

  void centergame::init() {
          require_auth(HOUSE_ACCOUNT);
          init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
 }
  // 101,1,abc
   void centergame::test(string memo) {
     //get 1 2 3      101,1,abc
   param_reader reader(memo);
     uint8_t game_id = reader.next_param_i("game_id is missing");
     uint8_t bet_id = reader.next_param_i("bet_id is missing");
     uint8_t content = reader.next_param_i("content is missing");
      std::printf("memo",game_id);
      std::printf("bet_id",bet_id);
      std::printf("content",content);
    }

void centergame::transfer(name from, name to, asset quantity, string memo) {
     if (!check_transfer(this, from, to, quantity, memo, false)) {
                  return;
      }
      eosio_assert(is_balance_within_range(quantity), "invalid quantity"); 
       uint64_t amount = quantity.symbol == EOS_SYMBOL ? quantity.amount : 0;
         //to house 最大支付额
	    transfer_to_house(_self, quantity, from, quantity.amount);
       //获取gameId
       //get referer
        param_reader reader(memo);
       //gameid|refer|自定义内容
      uint8_t game_id = reader.next_param_i("game_id is missing");
      name referer = reader.get_referer(from);
      freecontent = reader.next_param_i("freeContent is missing");
      
        uint64_t bet_id = increment_global(_globals, GLOBAL_ID_BET);

        uint32_t _now = now();
         eosio::time_point_sec time = eosio::time_point_sec( _now );
          bet_records.emplace(_self, [&](auto &a) {
          a.id = bet_id;
          a.player= from;
          a.game_id = game_id;
          a.bet_amount = amount;
          a.referer = referer;
          a.free_content=freecontent;
          a.time = time;});
}

//reward[]  rewardamount[] std::vector<int> 结算

void centergame::reveal(uint8_t game_id,std::vector<uint8_t>& betIds,std::vector<name>& reward_names,std::vector<uint8_t>& prize_amounts) {
   //get player info
   for (int i=0; i<3; i++) {
    uint8_t prize_amount= prize_amounts[i];
    uint8_t betid= betIds[i];
    name player=reward_names[i];
    //record find bet_id  bet_amount refer
    auto itr = bet_records.find(betid);
    action(
            permission_level{get_self(),name("active")},
            name("houseaccount"),                     
            name("pay"),                         
            std::make_tuple(game_id, player,itr->bet_amount,prize_amount, itr->referer)    
            ).send();
         
}
}


}
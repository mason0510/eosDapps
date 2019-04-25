#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "centergame.hpp"
#include "../common/utils.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/game_contracts.hpp"


#define BET_START 1
#define MAX_BET_AMOUNT 60000
#define MIN_BETAMOUNT 1000
#define EOS_SYMBOL symbol("EOS", 4)
#define GLOBAL_ID_START         1001
#define GLOBAL_ID_END         1004
#define EOs_SYMBOL symbol(symbol_code("EOS"), 4)
namespace godapp {
    centergame::centergame(name receiver, name code, datastream<const char *> ds) :
        contract(receiver, code, ds),
        _test_bets(_self, _self.value),
        _bets(_self, _self.value),
        _betprizes(_self, _self.value) {
        }
    
     void centergame::test(name account) {
           asset my_asset = asset(1000, EOS_SYMBOL);
            uint64_t next_bet_id = _bets.available_primary_key();
            _bets.emplace(get_self(), [&](auto &a) {
            a.id = next_bet_id;
            a.player= name(account);
            a.game_id=1000;
            a.bet_asset = my_asset;
            a.referer= name("houseaccount");
            a.self_content= "self_content";
            a.time = now();
            });
     }

   void centergame::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        if (!check_transfer(this, from, to, quantity, memo)) { 
            return; 
        }; 

       transfer_to_house(_self, quantity, from, quantity.amount); 
        print(from.to_string() );
        print(to.to_string());
        print("memo",memo.c_str() );

        param_reader reader(memo); 
        auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() ); 
        auto referer = reader.get_referer(from); 
        uint32_t timestamp = now(); 
        asset total = asset(0, quantity.symbol); 
        while(reader.has_next()) { 
            string self_content  = reader.next_param().c_str(); 
            uint64_t next_bet_id = _bets.available_primary_key();
            _bets.emplace(_self, [&](auto &a) { 
                a.id = next_bet_id; 
                a.game_id = game_id; 
                a.bet_asset=quantity;
                a.player = from; 
                a.referer = referer;
                a.self_content = self_content; 
                a.time = timestamp; 
            }); 
        } 
    }

void centergame::reveal(uint8_t game_id,std::vector<uint8_t>& betIds,std::vector<name>& reward_names,std::vector<asset>& prize_amounts) {
   require_auth(_self);
   for (int i=0; i<betIds.size(); i++) {
    asset prize_amount= prize_amounts[i];
    uint8_t betid= betIds[i];
    name player=reward_names[i];
     require_recipient( player );
    auto itr = _bets.find(betid);
    action(
            permission_level{_self,name("active")},
            name("houseaccount"),                     
            name("pay"),                         
            std::make_tuple(game_id, player,itr->bet_asset,prize_amount,"Dapp365 Game win!",itr->referer)    
            ).send(); 
}
}
}

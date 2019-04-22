#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "dice.hpp"
#include "../common/utils.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/game_contracts.hpp"

#define GLOBAL_ID_START 1001

#define GLOBAL_ID_BET 1001
#define GLOBAL_ID_HISTORY_INDEX 1002
#define GLOBAL_ID_END 1003

#define BET_HISTORY_LEN 40
#define MAX_ROLL_NUM 100

#define HOUSE_EDGE 2
#define MAX_BET 97
#define MIN_BET 1

using namespace std;
using namespace eosio;

namespace godapp {
    dice::dice(name receiver, name code, datastream<const char *> ds) :
    contract(receiver, code, ds),
    _globals(_self, _self.value),
    _active_bets(_self, _self.value) {
    }

    void dice::init() {
        require_auth(HOUSE_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    DEFINE_SET_GLOBAL(dice)


    asset reward_amount(asset bet_asset, uint8_t bet_number) {
        return bet_asset * (100 - HOUSE_EDGE) / bet_number;
    }




    void dice::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        param_reader reader(memo);
        auto bet_number = reader.next_param_i("Roll prediction cannot be empty!");
        eosio_assert(bet_number >= MIN_BET && bet_number <= MAX_BET, "bet number must between 1 to 97");
        transfer_to_house(_self, quantity, from, reward_amount(quantity, bet_number).amount);

        uint32_t _now = now();
        eosio::time_point_sec time = eosio::time_point_sec( _now );

        name referer = reader.get_referer(from);
        uint64_t bet_id = increment_global(_globals, GLOBAL_ID_BET);

        capi_checksum256 seed = create_seed(_self.value, bet_id);
        _active_bets.emplace(_self, [&](auto& a){
            a.id = bet_id;
            a.player = from;
            a.referer = referer;
            a.bet_asset = quantity;
            a.seed = seed;
            a.bet_number = bet_number;
            a.time = time;
        });
    }


    void dice::reveal(uint64_t bet_id, capi_signature sig){
        auto activebets_itr = _active_bets.find( bet_id );
        eosio_assert(activebets_itr != _active_bets.end(), "Bet doesn't exist");

        randkeys_index random_keys(HOUSE_ACCOUNT, HOUSE_ACCOUNT.value);
        auto key_entry = random_keys.get(0);
        random random_gen = random_from_sig(key_entry.key, activebets_itr->seed, sig);
        uint64_t roll_value = random_gen.generator(MAX_ROLL_NUM);

        asset payout;
        asset bet_asset = activebets_itr->bet_asset;
        uint8_t bet_number = activebets_itr->bet_number;
        name player = activebets_itr->player;

        if (roll_value < activebets_itr->bet_number) {
            payout = reward_amount(bet_asset, bet_number);
        } else {
            payout = bet_asset * 0;
        }

        uint64_t history_index = increment_global_mod(_globals, GLOBAL_ID_HISTORY_INDEX, BET_HISTORY_LEN);
        bet_index bets(_self, _self.value);
        table_upsert(bets, _self, history_index, [&](auto& a) {
            a.id = history_index;
            a.bet_id = bet_id;
            a.player = activebets_itr->player;

            a.sym = bet_asset.symbol;
            a.bet = (uint64_t) bet_asset.amount;
            a.payout = (uint64_t) payout.amount;

            a.bet_value = bet_number;
            a.roll_value = roll_value;
            a.time = activebets_itr->time;
        });
        delayed_action(_self, player, name("pay"), make_tuple(bet_id, player, bet_asset, payout, activebets_itr->seed,
                bet_number, roll_value, activebets_itr->referer), 0);
        _active_bets.erase(activebets_itr);
    }


    void dice::pay(uint64_t bet_id, name player, asset bet, asset payout,
            capi_checksum256 seed, uint8_t bet_value, uint64_t roll_value, name referer) {
        require_auth(_self);
        require_recipient( player );

        make_payment(_self, player, bet, payout, referer,
                     payout.amount > 0 ? "[Dapp365] Dice game win!" : "[Dapp365] Dice game lose!");
    }
}

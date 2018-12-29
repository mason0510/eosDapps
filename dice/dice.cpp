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
    void dice::init() {
        require_auth(HOUSE_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    DEFINE_SET_GLOBAL(dice)

    uint64_t reward_amount(asset bet_asset, uint8_t  bet_number) {
        return bet_asset.amount * (100 - HOUSE_EDGE) / bet_number;
    }

    void dice::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        param_reader reader(memo);
        auto bet_number = reader.next_param_i("Roll prediction cannot be empty!");
        eosio_assert(bet_number >= MIN_BET && bet_number <= MAX_BET, "bet number must between 1 to 97");
        transfer_to_house(_self, quantity, from, reward_amount(quantity, bet_number));

        name referer = reader.get_referer(from);
        uint64_t bet_id = increment_global(_globals, GLOBAL_ID_BET);
        delayed_action(_self, from, name("play"), make_tuple(bet_id, from, quantity, bet_number, referer));
    }

    void dice::play(uint64_t bet_id, name player, asset bet_asset, uint8_t bet_number, name referer){
        require_auth(_self);
        delayed_action(_self, player, name("resolve"), make_tuple(bet_id, player, bet_asset, bet_number, referer));
    }

    void dice::resolve(uint64_t bet_id, name player, asset bet_asset, uint8_t bet_number, name referer){
        require_auth(_self);

        random random_gen(bet_id);
        uint64_t roll_value = random_gen.generator(MAX_ROLL_NUM);
        capi_checksum256 seed = random_gen.get_seed();

        asset payout;
        uint32_t _now = now();
        if (roll_value < bet_number) {
            payout = asset(reward_amount(bet_asset, bet_number), bet_asset.symbol);
        } else {
            payout = asset(0, bet_asset.symbol);
        }

        eosio::time_point_sec time = eosio::time_point_sec( _now );
        uint64_t history_index = increment_global_mod(_globals, GLOBAL_ID_HISTORY_INDEX, BET_HISTORY_LEN);

        bet_index bets(_self, _self.value);
        table_upsert(bets, _self, history_index, [&](auto& a) {
            a.id = history_index;
            a.bet_id = bet_id;
            a.player = player;

            a.sym = bet_asset.symbol;
            a.bet = (uint64_t) bet_asset.amount;
            a.payout = (uint64_t) payout.amount;

            a.bet_value = bet_number;
            a.roll_value = roll_value;
            a.time = time;
        });
        delayed_action(_self, player, name("pay"), make_tuple(bet_id, player, bet_asset, payout, seed, bet_number, roll_value, referer), 0);
    }

    void dice::pay(uint64_t bet_id, name player, asset bet, asset payout,
            capi_checksum256 seed, uint8_t bet_value, uint64_t roll_value, name referer) {
        require_auth(_self);
        require_recipient( player );

        make_payment(_self, player, bet, payout, referer,
                     payout.amount > 0 ? "[GoDapp] Dice game win!" : "[GoDapp] Dice game lose!");
    }
}

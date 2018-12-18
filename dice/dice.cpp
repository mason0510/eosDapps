#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "dice.hpp"
#include "../house/house.hpp"
#include "../common/eosio.token.hpp"
#include "../common/utils.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"

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
        require_auth(TEAM_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    void dice::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        INLINE_ACTION_SENDER(eosio::token, transfer)(EOS_TOKEN_CONTRACT, {_self, name("active")},
                                                     {_self, HOUSE_ACCOUNT, quantity, from.to_string()});

        param_reader reader(memo);
        auto bet_number = reader.next_param_i("Roll prediction cannot be empty!");
        eosio_assert(bet_number >= MIN_BET && bet_number <= MAX_BET, "bet number must between 1 to 97");
        name referer = reader.get_referer(from);

        eosio::transaction r_out;
        r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, _self, name("resolve"),
                make_tuple(from, quantity, bet_number, referer));
        r_out.delay_sec = 1;
        r_out.send(_self.value, _self);
    }

    void dice::resolve(name player, asset bet_asset, uint8_t roll_type, uint8_t bet_number, name referer){
        require_auth(_self);

        random random_gen;
        uint64_t roll_value = random_gen.generator(MAX_ROLL_NUM);
        capi_checksum256 seed = random_gen.get_seed();

        uint64_t bet_id = increment_global(_globals, GLOBAL_ID_BET);
        asset payout;

        uint32_t _now = now();
        int64_t reward_amt;

        if (roll_value < bet_number) {
            reward_amt = bet_asset.amount * (100 - HOUSE_EDGE) / bet_number;
            payout = asset(reward_amt, bet_asset.symbol);
        } else {
            payout = asset(0, bet_asset.symbol);
        }

        char msg[128];
        sprintf(msg, "[GoDapp] Bet id: %lld. You win! ", bet_id);
        transaction deal_trx;
        deal_trx.actions.emplace_back(permission_level{ _self, name("active") }, HOUSE_ACCOUNT, name("pay"),
                                      make_tuple(_self, player, bet_asset, payout, string(msg), referer));
        deal_trx.delay_sec = 0;
        deal_trx.send(_self.value, _self);

        eosio::time_point_sec time = eosio::time_point_sec( _now );
        uint64_t history_index = increment_global_mod(_globals, GLOBAL_ID_HISTORY_INDEX, BET_HISTORY_LEN);

        bet_index bets(_self, _self.value);
        table_upsert(bets, _self, history_index, [&](auto& a) {
            a.id = history_index;
            a.bet_id = bet_id;
            a.player = player;
            a.referer = referer;

            a.sym = bet_asset.symbol;
            a.bet = (uint64_t) bet_asset.amount;
            a.payout = (uint64_t) payout.amount;

            a.bet_value = bet_number;
            a.roll_value = roll_value;
            a.seed = seed;
            a.time = time;
        });

        INLINE_ACTION_SENDER(dice, receipt)(_self, {_self, name("active")}, {bet_id, player, bet_asset, payout, seed, bet_number, roll_value});
    }

    void dice::receipt(uint64_t bet_id, name player, asset bet, asset payout,
            capi_checksum256 seed, uint64_t bet_value, uint64_t roll_value) {
        require_auth(_self);
        require_recipient( player );
    }
}

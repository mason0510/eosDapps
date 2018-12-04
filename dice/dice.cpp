#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>
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
#define ROLL_TYPE_UNDER 1
#define ROLL_TYPE_OVER 2

#define HOUSE_EDGE 2
#define MAX_BET 97
#define MIN_BET 2

using namespace std;
using namespace eosio;

namespace godapp {
    void dice::init() {
        require_auth(TEAM_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    void dice::play(name player, asset bet, string memo){
        require_auth(TEAM_ACCOUNT);

        param_reader reader(memo);
        auto roll_type = (uint8_t) atoi( reader.next_param("Roll type cannot be empty!").c_str() );
        auto bet_number = (uint8_t) atoi( reader.next_param("Roll prediction cannot be empty!").c_str() );
        eosio_assert(bet_number >= MIN_BET && bet_number <= MAX_BET, "Bet border must between 2 to 97");
        name referer = reader.get_referer(player);

        eosio::transaction r_out;
        r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, _self, name("resolve"),
                make_tuple(player, bet, roll_type, bet_number, referer));
        r_out.delay_sec = 1;
        r_out.send(player.value, _self);
    }

    int64_t get_bet_reward(uint8_t roll_type, uint8_t bet_number, int64_t amount){
        uint8_t fit_num;
        if (roll_type == ROLL_TYPE_UNDER) {
            fit_num = bet_number;
        } else {
            fit_num = (uint8_t) (MAX_ROLL_NUM - 1 - bet_number);
        }

        int64_t reward_amt = amount * (100 - HOUSE_EDGE) / fit_num;
        return reward_amt;
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

        if ( (roll_type == ROLL_TYPE_UNDER && roll_value < bet_number) || (roll_type == ROLL_TYPE_OVER && roll_value > bet_number) ) {
            reward_amt = get_bet_reward(roll_type, bet_number, bet_asset.amount);
            payout = asset(reward_amt, bet_asset.symbol);

            char str[128];
            sprintf(str, "Bet id: %lld. You win! ", bet_id);
            INLINE_ACTION_SENDER(house, pay)(HOUSE_ACCOUNT, {_self, name("active")}, {_self, player, payout, string(str), referer} );
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
            a.referer = referer;

            a.sym = bet_asset.symbol;
            a.bet = (uint64_t) bet_asset.amount;
            a.payout = (uint64_t) payout.amount;

            a.roll_type = roll_type;
            a.bet_value = bet_number;
            a.roll_value = roll_value;
            a.seed = seed;
            a.time = time;
        });

        INLINE_ACTION_SENDER(dice, receipt)(_self, {_self, name("active")}, {bet_id, player, bet_asset, payout, seed, roll_type, bet_number, roll_value});
    }

    void dice::receipt(uint64_t bet_id, name player, asset bet, asset payout,
            capi_checksum256 seed, uint8_t roll_type, uint64_t bet_value, uint64_t roll_value) {
        require_auth(_self);
        require_recipient( player );
    }
}

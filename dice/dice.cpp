#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "dice.hpp"
#include "../common/eosio.token.hpp"
#include "../common/utils.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"

#define GLOBAL_ID_START 1001

#define GLOBAL_ID_BET 1001
#define GLOBAL_ID_HISTORY_INDEX 1002
#define GLOBAL_ID_ACTIVE        1003

#define GLOBAL_ID_END 1003

#define BET_HISTORY_LEN 40
#define MAX_ROLL_NUM 100
#define ROLL_TYPE_UNDER 1
#define ROLL_TYPE_OVER 2

#define HOUSE_EDGE 2
#define REFERRAL_BONUS 0.005
#define SINGLE_BET_MAX_PERCENT 1.5
#define MAX_BET_PERCENT 200

#define MAX_BET 97
#define MIN_BET 2

using namespace std;
using namespace eosio;

namespace godapp {
    void dice::init() {
        require_auth(_self);
        global_index globals(_self, _self.value);
        init_globals(globals, GLOBAL_ID_START, GLOBAL_ID_END);

        token_index tokens(_self, _self.value);
        init_token(tokens, EOS_SYMBOL, EOS_TOKEN_CONTRACT);
    }

    void dice::setactive(bool active) {
        require_auth(_self);

        global_index globals(_self, _self.value);
        set_global(globals, GLOBAL_ID_ACTIVE, active);
    }

    void dice::start(name bettor, asset bet_asset, uint8_t roll_type, uint8_t bet_number, name referrer){
        require_auth(_self);

        eosio::transaction r_out;
        r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, _self, name("resolve"),
                make_tuple(bettor, bet_asset, roll_type, bet_number, referrer));
        r_out.delay_sec = 0;
        r_out.send(bettor.value, _self);
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

    void dice::resolve(name bettor, asset bet_asset, uint8_t roll_type, uint8_t bet_number, name referrer){
        require_auth(_self);

        token_index tokens(_self, _self.value);
        auto token_iter = tokens.find(bet_asset.symbol.raw());

        random random_gen;
        uint64_t roll_value = random_gen.generator(MAX_ROLL_NUM);
        capi_checksum256 seed = random_gen.get_seed();

        global_index globals(_self, _self.value);
        uint64_t bet_id = increment_global(globals, GLOBAL_ID_BET);
        vector<asset> payout_list;
        asset payout;

        uint32_t _now = now();
        int64_t reward_amt;

        if ( (roll_type == ROLL_TYPE_UNDER && roll_value < bet_number) || (roll_type == ROLL_TYPE_OVER && roll_value > bet_number) ) {
            reward_amt = get_bet_reward(roll_type, bet_number, bet_asset.amount);
            payout = asset(reward_amt, bet_asset.symbol);

            char str[128];
            sprintf(str, "Bet id: %lld. You win! ", bet_id);
            INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, bettor, payout, string(str)} );
        }
        else {
            reward_amt = 0;
            payout = asset(0, bet_asset.symbol);
        }

        payout_list.push_back(payout);

        eosio::time_point_sec time = eosio::time_point_sec( _now );
        save_bet(bet_id, bettor, referrer, bet_asset, payout_list, roll_type, bet_number, roll_value, seed, time);

        tokens.modify(token_iter, _self, [&](auto& a) {
            a.in += bet_asset.amount;
            a.play_times += 1;
            a.out += reward_amt;
        });

        INLINE_ACTION_SENDER(dice, receipt)(_self, {_self, name("active")}, {bet_id, bettor, bet_asset, payout_list, seed, roll_type, bet_number, roll_value});

        if (referrer != DICE_ACCOUNT && is_account(referrer)) {
            asset referral_reward = asset((uint64_t) (bet_asset.amount * REFERRAL_BONUS), bet_asset.symbol);
            char str[128];
            sprintf(str, "Referral reward from GO DAPP! Player: %s, Bet ID: %lld", eosio::name{bettor}.to_string().c_str(), bet_id);
            INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, referrer, referral_reward, string(str)} );
        }
    }

    void dice::receipt(uint64_t bet_id, name bettor, asset bet_amt, vector<asset> payout_list,
            capi_checksum256 seed, uint8_t roll_type, uint64_t roll_border, uint64_t roll_value) {
        require_auth(_self);
        require_recipient( bettor );
    }

    void dice::transfer(name from, name to, asset quantity, string memo) {
        if (check_transfer(this, from, to, quantity, memo)) {
            uint64_t amt = quantity.amount;

            auto balance = get_token_balance(_self, quantity.symbol);
            token_index tokens(_self, _self.value);
            auto token = tokens.get(quantity.symbol.raw(), "Token not supported");

            int64_t max = (balance.amount * SINGLE_BET_MAX_PERCENT / 100);
            eosio_assert(amt <= max, "Bet amount exceeds max amount.");
            eosio_assert(balance.amount >= token.protect, "Game under maintain, stay tuned.");

            param_reader reader(memo);
            auto roll_type = (uint8_t) atoi( reader.next_param("Roll type cannot be empty!").c_str() );
            auto bet_number = (uint8_t) atoi( reader.next_param("Roll prediction cannot be empty!").c_str() );
            eosio_assert(bet_number >= MIN_BET && bet_number <= MAX_BET, "Bet border must between 2 to 97");

            name referrer = reader.get_referrer(from);

            int64_t max_reward = get_bet_reward(roll_type, bet_number, amt);
            if (amt < token.min && max_reward > (balance.amount / MAX_BET_PERCENT)) {
                double max_bet_token = (amt * (balance.amount / MAX_BET_PERCENT) / max_reward) / 10000.0;
                double min_bet_token = (token.min) / 10000.0;
                char str[128];
                sprintf(str, "Bet amount must between %f and %f", min_bet_token, max_bet_token);
                eosio_assert(false, str);
            }

            eosio::transaction r_out;
            auto t_data = make_tuple(from, quantity, roll_type, bet_number, referrer);
            r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, _self, name("start"), t_data);
            r_out.delay_sec = 1;
            r_out.send(from.value, _self);
        }
    }

    void dice::save_bet(uint64_t bet_id, name bettor, name inviter, asset bet_quantity,
            const vector<eosio::asset>& payout_list, uint8_t roll_type, uint64_t bet_number, uint64_t roll_value,
            capi_checksum256 seed, eosio::time_point_sec time) {

        global_index globals(_self, _self.value);
        uint64_t history_index = increment_global_mod(globals, GLOBAL_ID_HISTORY_INDEX, BET_HISTORY_LEN);
        token_index tokens(_self, _self.value);
        auto token = tokens.get(bet_quantity.symbol.raw());

        bet_index bets(_self, _self.value);
        table_upsert(bets, _self, history_index, [&](auto& a) {
            a.id = history_index;
            a.bet_id = bet_id;
            a.contract = token.contract;
            a.bettor = bettor;
            a.inviter = inviter;

            a.bet_amt = (uint64_t) bet_quantity.amount;
            a.payout = payout_list;
            a.roll_type = roll_type;
            a.roll_border = bet_number;
            a.roll_value = roll_value;
            a.seed = seed;
            a.time = time;
        });
    }
}

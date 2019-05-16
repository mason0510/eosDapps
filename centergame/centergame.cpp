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
#include "../common/payment_map.hpp"

#define GLOBAL_ID_START         1001
#define GLOBAL_ID_BET_ID        1001
#define GLOBAL_ID_END           1001

namespace godapp {
    centergame::centergame(name receiver, name code, datastream<const char *> ds) :
        contract(receiver, code, ds),
        _globals(_self, _self.value),
        _bets(_self, _self.value) {
    }

    DEFINE_SET_GLOBAL(centergame)

    void centergame::init() {
        require_auth(HOUSE_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    void centergame::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        }

        transfer_to_house(_self, quantity, from, quantity.amount);
        param_reader reader(memo); 
        auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() );
        auto referer = reader.get_referer(from);
        auto message = reader.rest();

        uint32_t timestamp = now();
        uint64_t next_bet_id = increment_global(_globals, GLOBAL_ID_BET_ID);
        _bets.emplace(_self, [&](auto &a) {
            a.id = next_bet_id;
            a.game_id = game_id;
            a.bet_asset = quantity;
            a.player = from;
            a.referer = referer;
            a.self_content = message;
            a.time = timestamp;
        });
    }

    void centergame::reveal(uint64_t game_id, const std::string& message, const std::vector<uint64_t>& bet_ids,
        const std::vector<asset>& prize_amounts) {
        require_auth(_self);

        payment_map pay_map;
        name empty_referer;

        for (int i=0; i<bet_ids.size(); i++) {
            asset prize_amount = prize_amounts[i];
            uint64_t betId = bet_ids[i];
            auto itr = _bets.find(betId);
            eosio_assert(itr != _bets.end(), "Invalid bet Id");
            pay_map.add_payment(itr->player, itr->bet_asset, asset(prize_amount, itr->bet_asset.symbol), empty_referer);
            _bets.erase(itr);
        }

        for (auto& iter: pay_map.result_map) {
            name player(iter.first);
            const payment_map::pay_result& result = iter.second;
            delayed_action(_self, player, name("payment"), make_tuple(game_id, player,
                result.referer, message, result.bet, result.payout), 0);
        }
    }


    void centergame::clear() {
        require_auth(_self);
        for (auto iter = _bets.begin(); iter != _bets.end();) {
            iter = _bets.erase(iter);
        }
    }

    void centergame::payment(uint64_t id, name player, name referer, const std::string& message, asset bet, asset payout) {
        require_auth(_self);
        require_recipient(player);
        make_payment(_self, player, bet, payout, referer, message);
    }

}
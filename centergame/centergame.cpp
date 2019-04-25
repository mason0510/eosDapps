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

namespace godapp {
    centergame::centergame(name receiver, name code, datastream<const char *> ds) :
        contract(receiver, code, ds),
        _bets(_self, _self.value) {
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
        uint64_t next_bet_id = _bets.available_primary_key();
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

    void centergame::reveal(uint8_t game_id, const std::string& message, const std::vector<uint8_t>& bet_ids,
        const std::vector<asset>& prize_amounts) {
        require_auth(_self);
        for (int i=0; i<bet_ids.size(); i++) {
            asset prize_amount = prize_amounts[i];
            uint8_t betId = bet_ids[i];
            auto itr = _bets.find(betId);

            name player = itr->player;
            delayed_action(_self, player, name("payment"),
                make_tuple(game_id, player, itr->referer, message, itr->bet_asset, prize_amount), 0);

            _bets.erase(itr);
        }
    }

    void centergame::payment(uint64_t id, name player, name referer, const std::string& message, asset bet, asset payout) {
        require_auth(_self);
        require_recipient(player);

        make_payment(_self, player, bet, payout, referer, message);
    }
}
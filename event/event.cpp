//
// Created by Xiangwen Li on 2019-04-19.
//

#include "event.hpp"
#include "../common/param_reader.hpp"

#define GLOBAL_ID_START         1001
#define GLOBAL_ID_EVENT_ID      1001
#define GLOBAL_ID_BET_ID        1002
#define GLOBAL_ID_MAX_BET       1004
#define GLOBAL_ID_END           1004

namespace godapp {

    void event::init() {
        require_auth(_self);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }
    DEFINE_SET_GLOBAL(event)

    void event::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        }

        param_reader reader(memo);
        auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() );
        auto bet_type = (uint8_t) atol( reader.next_param("Bet type cannot be empty").c_str() );

        auto game_itr = _events.find(game_id);
        eosio_assert(game_itr != _events.end(), "Game does not exist");
        eosio_assert(game_itr->active, "Game is no longer active");
        eosio_assert(game_itr->resolve_time > now(), "Game betting phase ended");
        eosio_assert(game_itr->rates.size() > bet_type, "Invalid bet type");

        uint64_t max_bet = get_global(_globals, GLOBAL_ID_MAX_BET, 1000000);
        auto idx = _active_bets.get_index<name("byplayer")>();
        auto bet_itr = idx.find(from.value);
        auto existing_itr = idx.end();

        asset total_bet = quantity;

        for (auto itr = bet_itr; itr != idx.end(); itr++) {
            if (itr->player != from) {
                break;
            }
            if (itr->game_id == game_id) {
                total_bet += itr->bet_asset;
                if (itr->bet_type == bet_type) {
                    existing_itr = itr;
                }
            }
        }
        eosio_assert(total_bet.amount <= max_bet, "Bet amount exceed maximum");
        if (existing_itr != idx.end()) {
            idx.modify(existing_itr, _self, [&](auto &a) {
                a.bet_asset += quantity;
            });
        } else {
            uint64_t next_bet_id = increment_global(_globals, GLOBAL_ID_BET_ID);
            _active_bets.emplace(_self, [&](auto &a) {
                a.id = next_bet_id;
                a.game_id = game_id;
                a.player = from;
                a.bet_type = bet_type;
                a.bet_asset = quantity;
                a.time = now();
            });
        }

        _events.modify(game_itr, _self, [&](auto &a) {
           a.bets[bet_type] += quantity.amount;
        });
    }

    void event::addevent(const std::string& event_name, const std::vector<uint64_t>& rates, uint64_t resolve_time) {
        require_auth(_self);

        uint64_t next_event_id = increment_global(_globals, GLOBAL_ID_EVENT_ID);
        _events.emplace(_self, [&](auto &a) {
            a.id = next_event_id;
            a.event_name = event_name;
            a.resolve_time = resolve_time;
            a.rates = rates;
            a.bets = std::vector<uint64_t>(rates.size());
            a.active = false;
        });
    }

    void event::setactive(uint64_t id, const std::string& event_name, const std::vector<uint64_t>& rates, uint64_t resolve_time, bool active) {
        require_auth(_self);

        auto itr = _events.find(id);
        eosio_assert(itr != _events.end(), "Event does not exist");
        eosio_assert(itr->rates == rates, "Event rate does not match");
        eosio_assert(itr->resolve_time == resolve_time, "Resolve time does not match");
        _events.modify(itr, _self, [&](auto &a) {
            a.active = active;
        });
    }

    void event::closeevent(uint64_t id, const std::string& event_name, uint8_t result) {
        require_auth(_self);

        auto itr = _events.find(id);
        eosio_assert(itr != _events.end(), "Event does not exist");
        eosio_assert(result < itr->rates.size(), "Invalid event result");

        uint64_t payout = itr->rates[result] * itr->bets[result] / 100;
        _events.modify(itr, _self, [&](auto &a) {
            a.active = false;
            a.payout = payout;
        });
    }

    struct pay_result {
        asset bet;
        asset payout;
    };

    void event::resolve(uint64_t id, const std::string& event_name, uint8_t result, uint64_t payout) {
        require_auth(_self);

        auto event_itr = _events.find(id);
        eosio_assert(event_itr != _events.end(), "Event does not exist");
        eosio_assert(result < event_itr->rates.size(), "Invalid event result");
        eosio_assert(event_itr->payout != payout , "Invalid event payout number");
        uint64_t win_rate = event_itr->rates[result];

        auto idx = _active_bets.get_index<name("bygameid")>();
        auto bet_itr = idx.find(id);

        map<uint64_t, pay_result> result_map;
        for (auto itr = bet_itr; itr != idx.end(); itr++) {
            if (itr->game_id != id) {
                break;
            }
            uint8_t bet_type = itr->bet_type;
            asset bet_asset = itr->bet_asset;
            asset payout_amount = (bet_type == result) ? (bet_asset * win_rate / 100) : asset(0, EOS_SYMBOL);
            delayed_action(_self, bet_itr->player, name("payment"), make_tuple(id, bet_itr->player,
                event_itr->event_name, result, bet_itr->bet_asset, payout_amount), 0);
            idx.erase(bet_itr);
        }
        _events.erase(event_itr);
    }

    void event::payment(uint64_t id, name player, const std::string& event_name, uint8_t result, asset bet, asset payout) {
        require_auth(_self);
        require_recipient(player);

        if (payout.amount > 0) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(EOS_TOKEN_CONTRACT,
                                                         {_self, name("active")}, {_self, player, payout, "Dapp365 Event Win!"} );
        }
    }

}
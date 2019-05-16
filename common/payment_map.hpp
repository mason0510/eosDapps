#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

#include <map>
#include "constants.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    class payment_map {
    public:
        struct pay_result {
            asset bet;
            asset payout;
            name referer;
        };

        std::map<uint64_t, pay_result> result_map;
        uint64_t largest_winner = 0;
        int64_t win_amount = 0;

        void add_payment(name player, asset bet, asset payout, name referer) {
            auto result_itr = result_map.find(player.value);
            if (result_itr == result_map.end()) {
                result_map[player.value] = pay_result{bet, payout, referer};
            } else {
                result_itr->second.bet += bet;
                result_itr->second.payout += payout;
            }
        }

        void track_largest_winner(std::map<uint64_t, pay_result>::iterator itr) {
            pay_result& current = itr->second;
            if (current.payout.amount > current.bet.amount && current.payout.amount > win_amount &&
                current.payout.symbol == EOS_SYMBOL) {
                largest_winner = itr->first;
                win_amount = current.payout.amount;
            }
        }
    };
}
#pragma once

#include <eosiolib/eosio.hpp>
#include "../house/house.hpp"
#include "../common/eosio.token.hpp"

#include <string>

namespace godapp {
    using namespace std;
    using namespace eosio;

    void transfer_to_house(name self, asset quantity, name player, uint64_t max_payout) {
        // check that the token is supported and amount is within limit, update record accordingly
        house::token_index game_token(HOUSE_ACCOUNT, self.value);
        auto token_iter = game_token.find(quantity.symbol.raw());
        eosio_assert(token_iter != game_token.end(), "token is not supported");
        eosio_assert(quantity.amount >= token_iter->min && max_payout <= token_iter->max_payout, "amount not within the bet limit");
        INLINE_ACTION_SENDER(eosio::token, transfer)(EOS_TOKEN_CONTRACT, {self, name("active")},
                                                     {self, HOUSE_ACCOUNT, quantity, player.to_string()});
    }
}
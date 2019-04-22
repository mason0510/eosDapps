#pragma once

#include <eosiolib/eosio.hpp>
#include "../common/random.hpp"
#include "../house/house.hpp"
#include "../common/eosio.token.hpp"

#include <string>
#include <eosiolib/crypto.h>

namespace godapp {
    using namespace std;
    using namespace eosio;

#define DEFINE_BET_AMOUNT_TABLE \
        TABLE bet_amount { \
            name player; \
            asset bet; \
            uint64_t primary_key() const { return player.value; } \
        }; \
        typedef multi_index<name("betamount"), bet_amount> bet_amount_table; \
        bet_amount_table _bet_amount;


    void transfer_to_house(name self, asset quantity, name player, uint64_t max_payout) {
        // check that the token is supported and amount is within limit, update record accordingly
        house::token_index game_token(HOUSE_ACCOUNT, self.value);
        auto token_iter = game_token.find(quantity.symbol.raw());
        eosio_assert(token_iter != game_token.end(), "token is not supported");
        eosio_assert(quantity.amount >= token_iter->min && max_payout <= token_iter->max_payout, "amount not within the bet limit");
        INLINE_ACTION_SENDER(eosio::token, transfer)(EOS_TOKEN_CONTRACT, {self, name("active")},
                                                     {self, HOUSE_ACCOUNT, quantity, player.to_string()});
    }

    struct seed_data {
        uint64_t game;
        uint64_t game_id;
        int block_number;
        int block_prefix;
    };

    capi_checksum256 create_seed(uint64_t game, uint64_t bet_id) {
        capi_checksum256 seed;
        seed_data data{game, bet_id, tapos_block_num(), tapos_block_prefix()};
        sha256( (char *)&data.game, sizeof(data), &seed);

        return seed;
    }

    random random_from_sig(capi_public_key public_key, capi_checksum256 seed, capi_signature signature) {
        assert_recover_key(&seed, (const char *)&signature, sizeof(signature), (const char *)&public_key, sizeof(public_key));

        capi_checksum256 random_num_hash;
        sha256( (char *)&signature, sizeof(signature), &random_num_hash );
        return random(random_num_hash);
    }
}
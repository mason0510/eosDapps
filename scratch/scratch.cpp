#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "scratch.hpp"
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

#define RESULT_HIT      1
#define RESULT_PIG      2
#define RESULT_MISS     3

#define TYPE_BITS       4
#define RESULT_BITS     2

using namespace std;
using namespace eosio;

class reward_option {
public:
    uint64_t rate;
    uint64_t pigNumber;
    uint64_t winNumber;
};

reward_option rewards[] = {
    {1, 500, 45500},
    {2, 50, 4550},
    {5, 20, 1820},
    {10, 10, 910},
    {20, 5, 455},
    {50, 2, 182},
    {100, 1, 91},
    {1000, 0, 0},
    {10000, 0, 0},
};

namespace godapp {
    scratch::scratch(name receiver, name code, datastream<const char *> ds) :
    contract(receiver, code, ds),
    _globals(_self, _self.value),
    _active_cards(_self, _self.value) {
    }

    void scratch::init() {
        require_auth(HOUSE_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    DEFINE_SET_GLOBAL(scratch)


    void scratch::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        eosio_assert(quantity.amount == 1000 || quantity.amount == 10000, "Invalid amount");
        transfer_to_house(_self, quantity, from, quantity.amount);

        auto itr = _active_cards.find(from.value);
        eosio_assert(itr == _active_cards.end(), "You have an unclaimed card");

        uint32_t _now = now();
        eosio::time_point_sec time = eosio::time_point_sec( _now );

        param_reader reader(memo);
        name referer = reader.get_referer(from);
        uint64_t card_id = increment_global(_globals, GLOBAL_ID_BET);

        capi_checksum256 seed = create_seed(_self.value, card_id);
        _active_cards.emplace(_self, [&](auto& a){
            a.id = card_id;
            a.player = from;
            a.referer = referer;
            a.price = quantity;
            a.reward = asset(0, quantity.symbol);
            a.result = 0;
            a.seed = seed;
            a.time = time;
        });
    }

    void scratch::reveal(uint64_t card_id, capi_signature sig){
        auto active_card_itr = _active_cards.find( card_id);
        eosio_assert(active_card_itr != _active_cards.end(), "Card doesn't exist");
        eosio_assert(active_card_itr->result == 0, "Card has already been scratched");

        randkeys_index random_keys(HOUSE_ACCOUNT, HOUSE_ACCOUNT.value);
        auto key_entry = random_keys.get(0);
        random random_gen = random_from_sig(key_entry.key, active_card_itr->seed, sig);

        uint64_t reward_seed = random_gen.generator(0);
        uint64_t result = 0;
        asset reward(0, active_card_itr->price.symbol);

        for (int i=0; i<5; ++i) {
            uint8_t reward_type = reward_seed % 10;
            reward_type = min(0, reward_type - 1);
            reward_seed /= 10;

            result |= reward_type;
            result <<= TYPE_BITS;

            reward_option& option = rewards[reward_type];
            uint64_t roll = random_gen.generator(100000);
            uint8_t roll_result = RESULT_MISS;

            if (roll < option.pigNumber) {
                roll_result = RESULT_PIG;
                reward += active_card_itr->price * option.rate * 10;
            } else if (roll < option.winNumber) {
                roll_result = RESULT_HIT;
                reward += active_card_itr->price * option.rate;
            }
            result |= roll_result;
            result <<= RESULT_BITS;
        }

        delayed_action(_self, active_card_itr->player, name("pay"),
            make_tuple(card_id, active_card_itr->player, active_card_itr->price, reward, active_card_itr->seed,
                result, active_card_itr->referer), 0);
    }

    void scratch::pay(uint64_t card_id, name player, asset price, asset reward,
            capi_checksum256 seed, uint64_t result, name referer) {
        require_auth(_self);
        require_recipient( player );

        make_payment(_self, player, price, reward, referer, "[Dapp365] Scratch Card Reward!");
    }
}

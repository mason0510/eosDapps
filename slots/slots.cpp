#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "slots.hpp"
#include "../house/house.hpp"
#include "../common/utils.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/game_contracts.hpp"

#define GLOBAL_ID_START 1001

#define GLOBAL_ID_BET 1001
#define GLOBAL_ID_HISTORY_INDEX 1002
#define GLOBAL_ID_END 1003

#define BET_HISTORY_LEN 40
#define EMPTY_RESULT 65535
#define REWARD_COUNT 5

using namespace std;
using namespace eosio;

class reward_option {
public:
    uint16_t result;
    uint64_t winNumber;
};


reward_option rewards[] = {
    {2, 2000},
    {3, 3000},
    {5, 3300},
    {18, 3310},
    {88, 3311},
};

namespace godapp {
    slots::slots(name receiver, name code, datastream<const char *> ds) :
        contract(receiver, code, ds),
        _globals(_self, _self.value),
        _active_games(_self, _self.value){
    }

    void slots::init() {
        require_auth(HOUSE_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    DEFINE_SET_GLOBAL(slots)

    void slots::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        param_reader reader(memo);
        name referer = reader.get_referer(from);

        auto itr = _active_games.find(from.value);
        eosio_assert(itr == _active_games.end() || itr->result != 65535, "Game already in progress");

        uint32_t _now = now();
        eosio::time_point_sec time = eosio::time_point_sec( _now );
        uint64_t game_id = increment_global(_globals, GLOBAL_ID_BET);
        capi_checksum256 seed = create_seed(_self.value, game_id);

        table_upsert(_active_games, _self, from.value, [&](auto& game) {
            game.id = game_id;
            game.player = from;
            game.referer = referer;
            game.price = quantity;
            game.result = EMPTY_RESULT;
            game.seed = seed;
            game.time = time;
        });

        transfer_to_house(_self, quantity, from, quantity.amount);
    }

    void slots::reveal(uint64_t game_id, capi_signature sig){
        auto idx = _active_games.get_index<name("byid")>();
        auto itr = idx.find(game_id);
        eosio_assert(itr != idx.end(), "Card doesn't exist");
        eosio_assert(itr->result == EMPTY_RESULT, "Card has already been scratched");

        randkeys_index random_keys(HOUSE_ACCOUNT, HOUSE_ACCOUNT.value);
        auto key_entry = random_keys.get(0);
        random random_gen = random_from_sig(key_entry.key, itr->seed, sig);

        uint16_t result = 0;
        uint64_t value = random_gen.generator(10000);
        for (int i=0; i < REWARD_COUNT; i++) {
            reward_option& option = rewards[i];
            if (value < option.winNumber) {
                result = option.result;
                break;
            }
        }
        asset reward = itr->price * result;

        delayed_action(_self, itr->player, name("pay"),
            make_tuple(game_id, itr->player, itr->price, reward, itr->seed,
                result, itr->referer), 0);

        idx.modify(itr, _self, [&](auto& a) {
            a.id = game_id;
            a.result = result;
        });

        uint64_t history_index = increment_global_mod(_globals, GLOBAL_ID_HISTORY_INDEX, BET_HISTORY_LEN);
        history_table history(_self, _self.value);
        table_upsert(history, _self, history_index, [&](auto& a) {
            a.id = history_index;
            a.game_id = game_id;
            a.player = itr->player;
            a.price = itr->price;
            a.result = result;
            a.seed = itr->seed;
            a.time = itr->time;
        });
    }


    void slots::pay(uint64_t game_id, name player, asset price, asset reward,
            capi_checksum256 seed, uint16_t result, name referer) {
        require_auth(_self);
        require_recipient( player );

        INLINE_ACTION_SENDER(house, pay)(HOUSE_ACCOUNT, {_self, name("active")},
            {_self, player, price, reward, "[Dapp365] Slots Reward", referer});
    }
}

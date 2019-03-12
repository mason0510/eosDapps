#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>

#include "scratch.hpp"
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
#define MAX_ROLL_NUM 100

#define RESULT_HIT      1
#define RESULT_BONUS    2
#define RESULT_MISS     3

#define TYPE_BITS       4
#define RESULT_BITS     2

#define CARD_TYPE_COUNT 1

using namespace std;
using namespace eosio;

class reward_option {
public:
    uint64_t payout;
    uint64_t bonusNumber;
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

uint64_t prices[] = {1000};

namespace godapp {
    scratch::scratch(name receiver, name code, datastream<const char *> ds) :
    contract(receiver, code, ds),
    _globals(_self, _self.value),
    _active_cards(_self, _self.value),
    _available_cards(_self, _self.value) {
    }

    void scratch::init() {
        require_auth(HOUSE_ACCOUNT);
        init_globals(_globals, GLOBAL_ID_START, GLOBAL_ID_END);
    }

    DEFINE_SET_GLOBAL(scratch)

    string result_string(uint8_t line_result) {
        switch (line_result) {
            case RESULT_HIT:
                return "Hit";
            case RESULT_BONUS:
                return "Bonus";
            case RESULT_MISS:
                return "Miss";
            default:
                return "";
        }
    }

    void scratch::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        param_reader reader(memo);
        uint8_t card_type = reader.next_param_i();
        uint64_t price = reader.next_param_i64();
        uint64_t count = reader.next_param_i64();
        name referer = reader.get_referer(from);

        eosio_assert(card_type < CARD_TYPE_COUNT, "Invalid Card");
        eosio_assert(price == prices[card_type], "Invalid Price");
        eosio_assert(quantity.amount == price * count, "Invalid transfer amount");
        transfer_to_house(_self, quantity, from, quantity.amount);

        doClaim(from);

        auto itr = _active_cards.find(from.value);
        if(itr == _active_cards.end()) {
            scratch_card(from, card_type, asset(price, quantity.symbol), referer);
            count--;
        }

        if (count > 0) {
            table_upsert(_available_cards, _self, from.value, [&](auto& a) {
                a.player = from;
                a.card1_count += (card_type == 0 ? count : 0);
                a.card2_count += (card_type == 1 ? count : 0);
            });
        }
    }

    void scratch::play(name player, uint8_t card_type, name referer) {
        require_auth(player);

        doClaim(player);

        auto itr = _available_cards.find(player.value);
        eosio_assert(itr != _available_cards.end(), "You have no card available");

        eosio_assert(card_type < CARD_TYPE_COUNT, "Invalid Card");
        uint64_t price_amount = prices[card_type];
        if (card_type == 0 && itr->card1_count == 0) {
            eosio_assert(itr->card2_count > 0, "You have no card available");
            card_type = 1;
            price_amount = 10000;
        } else if (card_type == 1 && itr->card2_count == 0) {
            eosio_assert(itr->card1_count > 0, "You have no card available");
            card_type = 0;
            price_amount = 1000;
        }

        _available_cards.modify(itr, _self, [&](auto& a) {
                a.card1_count -= (card_type == 0 ? 1 : 0);
                a.card2_count -= (card_type == 1 ? 1 : 0);
        });
        scratch_card(player, card_type, asset(price_amount, EOS_SYMBOL), referer);
    }

    void scratch::scratch_card(name player, uint8_t card_type, asset price, name referer) {
        uint32_t _now = now();
        eosio::time_point_sec time = eosio::time_point_sec( _now );

        uint64_t card_id = increment_global(_globals, GLOBAL_ID_BET);

        capi_checksum256 seed = create_seed(_self.value, card_id);
        _active_cards.emplace(_self, [&](auto& a){
            a.id = card_id;
            a.player = player;
            a.referer = referer;
            a.price = price;
            a.card_type = card_type;
            a.reward = asset(0, price.symbol);
            a.result = 0;
            a.seed = seed;
            a.time = time;
        });
    }

    void scratch::reveal(uint64_t card_id, capi_signature sig){
        auto idx = _active_cards.get_index<name("byid")>();
        auto active_card_itr = idx.find(card_id);
        eosio_assert(active_card_itr != idx.end(), "Card doesn't exist");
        eosio_assert(active_card_itr->result == 0, "Card has already been scratched");

        randkeys_index random_keys(HOUSE_ACCOUNT, HOUSE_ACCOUNT.value);
        auto key_entry = random_keys.get(0);
        random random_gen = random_from_sig(key_entry.key, active_card_itr->seed, sig);

        uint64_t reward_seed = random_gen.generator(0);
        uint64_t result = 0;
        asset reward(0, active_card_itr->price.symbol);

        std::vector<line_result> result_detail;
        for (int i=0; i<5; ++i) {
            uint8_t reward_type = reward_seed % 10;
            reward_type = max(0, reward_type - 1);
            reward_seed /= 10;

            result <<= TYPE_BITS;
            result |= reward_type;

            reward_option& option = rewards[reward_type];
            uint64_t roll = random_gen.generator(100000);
            uint8_t roll_result = RESULT_MISS;

            if (roll < option.bonusNumber) {
                roll_result = RESULT_BONUS;
                reward += active_card_itr->price * option.payout * 10;
            } else if (roll < option.winNumber) {
                roll_result = RESULT_HIT;
                reward += active_card_itr->price * option.payout;
            }
            result <<= RESULT_BITS;
            result |= roll_result;

            result_detail.push_back({option.payout, result_string(roll_result)});
        }

        delayed_action(_self, active_card_itr->player, name("receipt"),
            make_tuple(card_id, active_card_itr->player, active_card_itr->price, reward, active_card_itr->seed,
                result_detail, active_card_itr->referer), 0);

        idx.modify(active_card_itr, _self, [&](auto& a) {
            a.id = card_id;
            a.reward = reward;
            a.result = result;
        });

        uint64_t history_index = increment_global_mod(_globals, GLOBAL_ID_HISTORY_INDEX, BET_HISTORY_LEN);
        history_table history(_self, _self.value);
        table_upsert(history, _self, history_index, [&](auto& a) {
            a.id = history_index;
            a.card_id = card_id;
            a.player = active_card_itr->player;
            a.price = active_card_itr->price;
            a.reward = reward;
            a.result = result;
            a.card_type = active_card_itr->card_type;
            a.seed = active_card_itr->seed;
            a.time = active_card_itr->time;
        });
    }

    void scratch::claim(name player){
        require_auth(player);
        doClaim(player);
    }

    void scratch::doClaim(name player){
        auto itr = _active_cards.find(player.value);
        if (itr != _active_cards.end()) {
            eosio_assert(itr->result != 0, "Card not revealed yet");

            INLINE_ACTION_SENDER(house, pay)(HOUSE_ACCOUNT, {_self, name("active")},
                {_self, player, itr->price, itr->reward, "[Dapp365] Scratch Card Reward", itr->referer});
            _active_cards.erase(itr);
        }
    }

    void scratch::receipt(uint64_t card_id, name player, asset price, asset reward,
            capi_checksum256 seed, std::vector<line_result> result, name referer) {
        require_auth(_self);
        require_recipient( player );
    }
}

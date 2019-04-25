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

#define CARD_TYPE_COUNT 4

using namespace std;
using namespace eosio;

class reward_option {
public:
    uint64_t payout;
    uint64_t bonusNumber;
    uint64_t winNumber;
};

reward_option rewards1[] = {
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

uint8_t card2_type[] = {
    0, 0, 0, 1, 1, 1, 1, 1, 1, 2,
    2, 3, 3, 4, 4, 5, 6, 7, 8, 9
};
reward_option rewards2_1[] = {
    {1, 0, 66667},
    {2, 0, 33333},
    {5, 0, 4000},
    {10, 0, 2000},
    {20, 0, 1000},
    {50, 0, 800},
    {100, 0, 400},
    {1000, 0, 0},
    {10000, 0, 0},
    {40000, 0, 0},
};

reward_option rewards2_2[] = {
    {1, 0, 16667},
    {2, 0, 8333},
    {5, 0, 1000},
    {10, 0, 500},
    {20, 0, 250},
    {50, 0, 200},
    {100, 0, 100},
    {1000, 0, 0},
    {10000, 0, 0},
    {40000, 0, 0},
};

reward_option rewards3[] = {
    // step 1
    {10, 1, 10000},
    {0, 2, 47000},
    {40, 3, 48000},
    {0, 5, 98000},
    {30, 6, 100000},

    // step 2
    {15, 7, 2000},
    {0, 9, 52000},
    {10, 10, 61000},
    {100, 11, 61500},
    {0, 12, 97700},
    {500, 13, 97800},
    {250, 14, 98000},
    {15, 15, 100000},

    // step 3
    {0, 16, 50000},
    {50, 17, 52000},
    {0, 19, 100000},

    // step 4
    {0, 21, 90000},
    {10, 22, 100000},

    // step 5
    {0, 24, 99620},
    {500, 25, 99700},
    {200, 26, 100000},
};

uint8_t card3_step[] = {
    0, 5, 13, 16, 18, 21
};

uint8_t card4_type[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3,
    4, 4, 5, 5, 6, 6, 7, 7, 8, 9
};

reward_option rewards4[] = {
    {1, 0, 50000},
    {2, 0, 20000},
    {4, 0, 5000},
    {6, 0, 100},
    {10, 0, 100},
    {20, 0, 20},
    {30, 0, 0},
    {100, 0, 0},
    {200, 0, 0},
    {20000, 0, 0},
};

uint64_t prices[] = {1000, 5000, 10000, 50000};

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
    //free send ths cards to this player
    void scratch::secretsend(name player){
        require_auth(get_self());
        auto itr = _available_cards.find(player.value);
        if (itr != _available_cards.end() && itr->card1_count > 0) {
            eosio_assert(false, "Already has a card");
        }else{
        table_upsert(_available_cards, _self, player.value, [&](auto& a) {
            a.player = player;
            a.card1_count = 1;
        });
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
        eosio_assert(count <= 100, "Maximum 100 cards allowed");
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
                a.card3_count += (card_type == 2 ? count : 0);
                a.card4_count += (card_type == 3 ? count : 0);
            });
        }
    }

    uint32_t get_card_count(uint8_t card_type, const scratch::available_card& cards) {
        switch (card_type) {
            case 0:
                return cards.card1_count;
            case 1:
                return cards.card2_count;
            case 2:
                return cards.card3_count;
            case 3:
                return cards.card4_count;
            default:
                eosio_assert(false, "Invalid card");
                return 0;
        }
    }

    void scratch::play(name player, uint8_t card_type, name referer) {
        require_auth(player);

        doClaim(player);

        auto itr = _available_cards.find(player.value);
        eosio_assert(itr != _available_cards.end(), "You have no card available");

        eosio_assert(card_type < CARD_TYPE_COUNT, "Invalid Card Type");
        uint64_t price_amount = prices[card_type];
        uint32_t card_count = get_card_count(card_type, *itr);
        if (card_count == 0) {
            for (int i = 0; i < CARD_TYPE_COUNT; i++) {
                if (i != card_type) {
                    card_count = get_card_count(i, *itr);
                    if (card_count > 0) {
                        card_type = i;
                        price_amount = prices[card_type];
                        break;
                    }
                }
            }
        }
        eosio_assert(card_count > 0, "You have no card available");

        _available_cards.modify(itr, _self, [&](auto& a) {
                a.card1_count -= (card_type == 0 ? 1 : 0);
                a.card2_count -= (card_type == 1 ? 1 : 0);
                a.card3_count -= (card_type == 2 ? 1 : 0);
                a.card4_count -= (card_type == 3 ? 1 : 0);
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

    uint64_t resolveCard1(random& random_gen, std::vector<scratch::line_result>& result_detail, const asset& price, asset& reward) {
        uint64_t reward_seed = random_gen.generator(0);
        uint64_t result = 0;
        for (int i=0; i<5; ++i) {
            uint8_t reward_type = reward_seed % 10;
            reward_type = max(0, reward_type - 1);
            reward_seed /= 10;

            result <<= TYPE_BITS;
            result |= reward_type;

            reward_option& option = rewards1[reward_type];
            uint64_t roll = random_gen.generator(100000);
            uint8_t roll_result = RESULT_MISS;

            if (roll < option.bonusNumber) {
                roll_result = RESULT_BONUS;
                reward += price * option.payout * 10;
            } else if (roll < option.winNumber) {
                roll_result = RESULT_HIT;
                reward += price * option.payout;
            }
            result <<= RESULT_BITS;
            result |= roll_result;

            result_detail.push_back({option.payout, result_string(roll_result)});
        }
        return result;
    }

    uint64_t resolveCard2(random& random_gen, std::vector<scratch::line_result>& result_detail, const asset& price, asset& reward) {
        uint64_t reward_seed = random_gen.generator(0);
        uint64_t result = 0;
        for (int i=0; i<5; ++i) {
            uint8_t reward_type = card2_type[reward_seed % 20];
            reward_seed /= 20;

            result <<= TYPE_BITS;
            result |= reward_type;

            reward_option& option = i == 0 ? rewards2_1[reward_type] : rewards2_2[reward_type];
            uint64_t roll = random_gen.generator(100000);
            uint8_t roll_result = RESULT_MISS;
            if (roll < option.winNumber) {
                roll_result = RESULT_HIT;
                reward += price * option.payout;
            }
            result <<= RESULT_BITS;
            result |= roll_result;

            result_detail.push_back({option.payout, result_string(roll_result)});
        }
        return result;
    }

    uint64_t resolveCard3(random& random_gen, std::vector<scratch::line_result>& result_detail, const asset& price, asset& reward) {
        uint64_t result = 0;
        uint8_t  currentStep = 0;
        for (int i=0; i<5; ++i) {
            uint64_t roll = random_gen.generator(100000);

            uint8_t steps = 0;
            uint64_t payout = 0;

            for (int j=card3_step[i]; j<card3_step[i+1]; j++) {
                auto option = rewards3[j];
                if (roll < option.winNumber) {
                    steps = ((uint8_t) option.bonusNumber) - currentStep;
                    currentStep = (uint8_t) option.bonusNumber;
                    payout = option.payout;
                    break;
                }
            }
            result <<= TYPE_BITS;
            result |= steps;

            uint8_t roll_result = RESULT_MISS;
            if (payout > 0) {
                roll_result = RESULT_HIT;
                reward += price * payout / 10;
            }
            result <<= RESULT_BITS;
            result |= roll_result;

            result_detail.push_back({currentStep, std::to_string(((float_t) payout) / 10)});
        }
        return result;
    }

    uint64_t resolveCard4(random& random_gen, std::vector<scratch::line_result>& result_detail, const asset& price, asset& reward) {
        uint64_t reward_seed = random_gen.generator(0);
        uint64_t result = 0;
        for (int i=0; i<5; ++i) {
            uint8_t reward_type = card4_type[reward_seed % 20];
            reward_seed /= 20;

            result <<= TYPE_BITS;
            result |= reward_type;

            reward_option& option = rewards4[reward_type];
            uint64_t roll = random_gen.generator(100000);
            uint8_t roll_result = RESULT_MISS;
            if (roll < option.winNumber) {
                roll_result = RESULT_HIT;
                reward += price * option.payout;
            }
            result <<= RESULT_BITS;
            result |= roll_result;

            result_detail.push_back({option.payout * 5, result_string(roll_result)});
        }
        return result;
    }

    void scratch::reveal(uint64_t card_id, capi_signature sig){
        auto idx = _active_cards.get_index<name("byid")>();
        auto active_card_itr = idx.find(card_id);
        eosio_assert(active_card_itr != idx.end(), "Card doesn't exist");
        eosio_assert(active_card_itr->result == 0, "Card has already been scratched");

        randkeys_index random_keys(HOUSE_ACCOUNT, HOUSE_ACCOUNT.value);
        auto key_entry = random_keys.get(0);
        random random_gen = random_from_sig(key_entry.key, active_card_itr->seed, sig);

        uint64_t result = 0;
        asset reward(0, active_card_itr->price.symbol);
        std::vector<line_result> result_detail;

        switch (active_card_itr->card_type) {
            case 0:
                result = resolveCard1(random_gen, result_detail, active_card_itr->price, reward);
                break;
            case 1:
                result = resolveCard2(random_gen, result_detail, active_card_itr->price, reward);
                break;
            case 2:
                result = resolveCard3(random_gen, result_detail, active_card_itr->price, reward);
                break;
            case 3:
                result = resolveCard4(random_gen, result_detail, active_card_itr->price, reward);
                break;
            default:
                eosio_assert(false, "Invalid Card Type");
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
//finished 
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

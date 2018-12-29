#include "../redblack/redblack.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define G_ID_START                  101
#define G_ID_RESULT_ID              101
#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_END                    103

#define GAME_LENGTH                 20
#define GAME_RESOLVE_TIME           5

#define RESULT_SIZE                 50

#define NUM_CARDS                   52

#define BET_RED_WIN                 1
#define BET_BLACK_WIN               2
#define BET_LUCKY_STRIKE            4

// special case, small pair still counts as a pair in turns of hand comparison
// but does not count as a lucky strike
#define HAND_SMALL_PAIR             1
#define HAND_PAIR                   2
#define HAND_STRAIGHT               3
#define HAND_FLUSH                  4
#define HAND_STRAIGHT_FLUSH         5
#define HAND_THREE_OF_A_KIND        6

#define RATE_WIN                    196
#define RATE_PAIR                   2
#define RATE_STRAIGHT               3
#define RATE_FLUSH                  3
#define RATE_STRAIGHT_FLUSH         15
#define RATE_THREE_OF_A_KIND        30


namespace godapp {
    redblack::redblack(name receiver, name code, datastream<const char*> ds):
    contract(receiver, code, ds),
    _globals(_self, _self.value),
    _games(_self, _self.value),
    _bets(_self, _self.value),
    _results(_self, _self.value){
    }

    DEFINE_STANDARD_FUNCTIONS(redblack)

    uint8_t get_lucky_strike_rate(uint8_t bet_type) {
        switch (bet_type) {
            case HAND_PAIR:
                return RATE_PAIR;
            case HAND_STRAIGHT:
                return RATE_STRAIGHT;
            case HAND_FLUSH:
                return RATE_FLUSH;
            case HAND_STRAIGHT_FLUSH:
                return RATE_STRAIGHT_FLUSH;
            case HAND_THREE_OF_A_KIND:
                return RATE_THREE_OF_A_KIND;
            default:
                return 0;
        }
    }

    bool is_same_suit(const vector<card_t> &hand) {
        uint8_t suit = card_suit(hand[0]);
        return suit == card_suit(hand[1]) && suit == card_suit(hand[2]);
    }

    /**
     * preprocess the points in a hand to convert them to points, and sort based on the point value
     */
    vector<uint8_t> preprocess_hand(const vector<card_t> &hand) {
        vector<uint8_t> result;
        for (uint8_t value : hand) {
            result.push_back(card_value_with_ace(value));
        }
        sort(result.begin(), result.end());
        return result;
    }

    uint8_t get_hand_type(const vector<uint8_t>& hand_points, bool same_suit) {
        bool straight = true, three_of_a_kind = true, pair = false;
        uint8_t last_value = hand_points[0];

        for (int i=1; i<3; i++) {
            uint8_t value = hand_points[i];
            if (last_value == value) {
                pair = true;
            } else {
                three_of_a_kind = false;
            }
            // special case for straight, ace can be in either 23A or QKA
            if (value - last_value != 1 && (value != ACE_HIGH_VALUE || last_value != 3)) {
                straight = false;
            }
            last_value = value;
        }

        if (three_of_a_kind) {
            return HAND_THREE_OF_A_KIND;
        } else if (straight) {
            return same_suit ? HAND_STRAIGHT_FLUSH : HAND_STRAIGHT;
        } else if (same_suit) {
            return HAND_FLUSH;
        } else if (pair) {
            return hand_points[1] < 9 ? HAND_SMALL_PAIR : HAND_PAIR;
        } else {
            return 0;
        }
    }

    uint8_t compare_side(uint8_t red, uint8_t black) {
        if (red > black) {
            return BET_RED_WIN;
        } else if (red < black) {
            return BET_BLACK_WIN;
        } else {
            return 0;
        }
    }

    /**
     * Compare hands with the same hand type
     */
    uint8_t compare_same_type(uint8_t hand_type, const vector<card_t>& red, const vector<card_t>& black) {
        switch (hand_type) {
            case HAND_THREE_OF_A_KIND:
                return compare_side(red[0], black[0]);
            case HAND_STRAIGHT_FLUSH :
            case HAND_STRAIGHT:
                // compare the largest card, with special handling for the case of 23A
                return compare_side((red[0] == 2 && red[2] == ACE_HIGH_VALUE)? red[1] : red[2],
                                    (black[0] == 2 && black[2] == ACE_HIGH_VALUE)? black[1] : black[2]);
            case HAND_PAIR:
            case HAND_SMALL_PAIR:{
                // the middle one must be in the pair
                uint8_t result = compare_side(red[1], black[1]);
                if (result != 0) {
                    return result;
                } else {
                    // compare the other card otherwise
                    return compare_side(
                            (red[0] == red[1]) ? red[2] : red[0],
                            (black[0] == black[1]) ? black[2] : black[0]);
                }
            }
            default: {
                for(int i=2; i>=0; i--) {
                    uint8_t result = compare_side(red[i], black[i]);
                    if (result != 0) {
                        return result;
                    }
                }
                return 0;
            }
        }
    }


    void redblack::reveal(uint64_t game_id) {
        require_auth(_self);

        auto idx = _games.get_index<name("byid")>();
        auto gm_pos = idx.find(game_id);
        uint32_t timestamp = now();

        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!");
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp >= gm_pos->end_time, "Can not reveal yet");

        vector<card_t> cards, red_cards, black_cards;
        random random_gen;
        add_cards(random_gen, red_cards, cards, 3, NUM_CARDS);
        add_cards(random_gen, black_cards, cards, 3, NUM_CARDS);

        vector<uint8_t> red_points = preprocess_hand(red_cards);
        vector<uint8_t> black_points = preprocess_hand(black_cards);

        uint8_t red_type = get_hand_type(red_points, is_same_suit(red_cards));
        uint8_t black_type = get_hand_type(black_points, is_same_suit(black_cards));

        uint8_t result = compare_side(red_type, black_type);
        if (result == 0) {
            result = compare_same_type(red_type, red_points, black_points);
        }
        uint8_t lucky_rate = get_lucky_strike_rate(max(red_type, black_type));

        map<uint64_t, pay_result> result_map;
        auto bet_index = _bets.get_index<name("bygameid")>();
        for (auto itr = bet_index.begin(); itr != bet_index.end();) {
            uint8_t bet_type = itr->bet_type;
            asset bet = itr->bet;

            asset payout;
            if (bet_type == result) {
                payout = bet * RATE_WIN / 100;
            } else if (bet_type == BET_LUCKY_STRIKE) {
                payout = bet * lucky_rate;
            }

            if (payout.amount > 0) {
                auto result_itr = result_map.find(itr->player.value);
                if (result_itr == result_map.end()) {
                    result_map[itr->player.value] = pay_result{bet, payout, itr->referer};
                } else {
                    result_itr->second.bet += bet;
                    result_itr->second.payout += payout;
                }
            }
            itr = bet_index.erase(itr);
        }

        auto largest_winner = result_map.end();
        int64_t win_amount = 0;
        for (auto itr = result_map.begin(); itr != result_map.end(); itr++) {
            auto current = itr->second;
            if (current.payout.amount > win_amount && current.payout.symbol == EOS_SYMBOL) {
                largest_winner = itr;
                win_amount = current.payout.amount;
            }
            make_payment(_self, name(itr->first), current.bet, current.payout, current.referer, "[GoDapp] War of Stars win!");
        }

        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID);
        name winner_name = largest_winner == result_map.end() ? name() : name(largest_winner->first);
        idx.modify(gm_pos, _self, [&](auto &a) {
            a.id = next_game_id;
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME;
            a.largest_winner = winner_name;
            a.largest_win_amount = asset(win_amount, EOS_SYMBOL);
            a.red_cards = red_cards;
            a.black_cards = black_cards;
        });

        if (lucky_rate > 0) {
            result |= BET_LUCKY_STRIKE;
        }
        uint64_t result_index = increment_global_mod(_globals, G_ID_RESULT_ID, RESULT_SIZE);
        table_upsert(_results, _self, result_index, [&](auto &a) {
            a.id = result_index;
            a.game_id = game_id;
            a.result = result;
        });
    }
};
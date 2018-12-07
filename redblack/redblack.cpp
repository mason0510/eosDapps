#include "../redblack/redblack.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define G_ID_START                  101
#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_END                    103

#define GAME_LENGTH                 30
#define GAME_RESOLVE_TIME           10

#define NUM_CARDS                   52

#define BET_RED_WIN                 1
#define BET_BLACK_WIN               2
#define BET_PAIR                    4
#define BET_STRAIGHT                8
#define BET_FLUSH                   16
#define BET_STRAIGHT_FLUSH          32
#define BET_THREE_OF_A_KIND         64

#define RATE_WIN                    1.96
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

    double payout_rate(uint8_t bet_type) {
        switch (bet_type) {
            case BET_RED_WIN:
            case BET_BLACK_WIN:
                return RATE_WIN;
            case BET_PAIR:
                return RATE_PAIR;
            case BET_STRAIGHT:
                return RATE_STRAIGHT;
            case BET_FLUSH:
                return RATE_FLUSH;
            case BET_STRAIGHT_FLUSH:
                return RATE_STRAIGHT_FLUSH;
            case BET_THREE_OF_A_KIND:
                return RATE_THREE_OF_A_KIND;
            default:
                return 0;
        }
    }

    bool is_same_suit(const vector<card_t> &hand) {
        uint8_t suit = card_suit(hand[0]);
        return suit == card_suit(hand[1]) && suit == card_suit(hand[2]);
    }

    vector<uint8_t> get_hand_points(const vector<card_t>& hand) {
        vector<uint8_t> result(3);
        for (uint8_t value : hand) {
            result.push_back(card_value_with_ace(value));
        }
        sort(result.begin(), result.end());
        return result;
    }

    uint8_t get_hand_type(const vector<uint8_t>& hand_points, bool same_suit) {
        bool straight = true, three_of_a_kind = true, pair = false;
        uint8_t last_value = hand_points[0];

        for (int i=1; i<2; i++) {
            uint8_t value = hand_points[i];
            if (last_value == value) {
                pair = true;
            } else {
                three_of_a_kind = false;
            }

            if (value - last_value != 1 && (value != 13 || last_value != 3)) {
                straight = false;
            }
        }

        if (three_of_a_kind) {
            return BET_THREE_OF_A_KIND;
        } else if (straight) {
            return same_suit ? BET_STRAIGHT_FLUSH : BET_STRAIGHT;
        } else if (same_suit) {
            return BET_FLUSH;
        } else if (pair) {
            return BET_PAIR;
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

    uint8_t compare_same_type(uint8_t hand_type, const vector<card_t>& red, const vector<card_t>& black) {
        switch (hand_type) {
            case BET_THREE_OF_A_KIND:
            case BET_STRAIGHT_FLUSH :
            case BET_STRAIGHT:
                // the largest card represents the overall strength
                return compare_side(red[2], black[2]);
            case BET_PAIR: {
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

    uint8_t get_result(const vector<card_t>& red, const vector<card_t>& black) {
        vector<uint8_t> red_points = get_hand_points(red);
        vector<uint8_t> black_points = get_hand_points(black);

        uint8_t red_type = get_hand_type(red_points, is_same_suit(red));
        uint8_t black_type = get_hand_type(black_points, is_same_suit(black));

        uint8_t result = max(red_type, black_type);
        if (red_type > black_type) {
            return result | BET_RED_WIN;
        } else if (black_type > red_type) {
            return result | BET_BLACK_WIN;
        } else {
            return result | compare_same_type(result, red_points, black_points);
        }
    }

    void redblack::reveal(uint64_t game_id) {
        require_auth(_self);

        auto idx = _games.get_index<name("byid")>();
        auto gm_pos = idx.find(game_id);
        uint32_t timestamp = now();

        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!");
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp >= gm_pos->end_time, "Can not reveal yet");

        vector<card_t> cards(6), red_cards(3), black_cards(3);
        random random_gen;
        add_cards(random_gen, red_cards, cards, 3, NUM_CARDS);
        add_cards(random_gen, black_cards, cards, 3, NUM_CARDS);

        uint8_t result = get_result(red_cards, black_cards);
        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID);
        idx.modify(gm_pos, _self, [&](auto &a) {
            a.id = next_game_id;
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME;

            a.red_cards = red_cards;
            a.black_cards = black_cards;
        });

        DEFINE_PAYMENT_BLOCK("Red Vs. Black")
    }
};
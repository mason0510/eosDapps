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
#define G_ID_HISTORY_ID             104
#define G_ID_END                    104

#define GAME_LENGTH                 40
#define GAME_RESOLVE_TIME           15

#define RESULT_SIZE                 60
#define HISTORY_SIZE                40

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

    class redblack_result {
    public:
        vector<card_t> red_cards, black_cards;
        vector<uint8_t> red_points, black_points;
        uint8_t red_type, black_type;
        uint8_t game_result, result;
        uint8_t lucky_rate;
        uint8_t roundResult;

        redblack_result(random& random_gen) {
            vector<card_t> cards;
            add_cards(random_gen, red_cards, cards, 3, NUM_CARDS);
            add_cards(random_gen, black_cards, cards, 3, NUM_CARDS);

            red_points = preprocess_hand(red_cards);
            black_points = preprocess_hand(black_cards);

            red_type = get_hand_type(red_points, is_same_suit(red_cards));
            black_type = get_hand_type(black_points, is_same_suit(black_cards));

            game_result = compare_side(red_type, black_type);
            if (game_result == 0) {
                game_result = compare_same_type(red_type, red_points, black_points);
            }
            lucky_rate = get_lucky_strike_rate(max(red_type, black_type));
            result = (lucky_rate > 0) ? (game_result | BET_LUCKY_STRIKE) : game_result;
            roundResult = result;
        }

        asset get_payout(const redblack::bet &bet_item) {
            asset bet = bet_item.bet;
            uint8_t bet_type = bet_item.bet_type;

            if (bet_type == BET_LUCKY_STRIKE) {
                return bet * lucky_rate;
            } else if (bet_type == game_result) {
                return bet * RATE_WIN / 100;
            } else {
                return bet * 0;
            }
        }

        string result_string() {
            switch (game_result) {
                case BET_RED_WIN:
                    return "Red Win";
                case BET_BLACK_WIN:
                    return "Blue Win";
                default:
                    return "Tie";
            }
        }

        void update_game(redblack::game &game) {
            game.red_cards = red_cards;
            game.black_cards = black_cards;
        }

        void set_receipt(redblack &contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, { contract.get_self(), name("active") }, { game_id, seed,
                cards_to_string(red_cards),
                cards_to_string(black_cards), result_string(), lucky_rate > 0 });
        }
    };

    void redblack::receipt(uint64_t game_id, capi_checksum256 seed, string red_cards, string blue_cards,
        string result, bool lucky_strike) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(redblack, Red Vs Blue, redblack_result, 1)
};
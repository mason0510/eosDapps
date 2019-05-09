#include "../bullfight/bullfight.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"
#include "../common/cards.hpp"

#define G_ID_START                  101
#define G_ID_RESULT_ID              101
#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_HISTORY_ID             104
#define G_ID_END                    104

#define GAME_LENGTH                 45
#define GAME_RESOLVE_TIME           25

#define RESULT_SIZE                 100
#define HISTORY_SIZE                100

#define NUM_CARDS                   52

#define  NO_BULL        0
#define  BULL_1         1
#define  BULL_2         2
#define  BULL_3         3
#define  BULL_4         4
#define  BULL_5         5
#define  BULL_6         6
#define  BULL_7         7
#define  BULL_8         8
#define  BULL_9         9
#define  BULL_10        10
#define  FOUR_OF_A_KIND 11
#define  ALL_HEADS      12
#define  SMALL_BULL     13

namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(bullfight)

    class bullfight_result {
    public:
        std::vector<card_t> banker_cards, player1_cards, player2_cards, player3_cards, player4_cards;
        std::vector<uint8_t> banker_card_values;

        uint8_t banker_type, player1_type, player2_type, player3_type, player4_type;
        int64_t player1_rate, player2_rate, player3_rate, player4_rate;
        uint64_t result = 0;
        uint8_t roundResult = 0;

        bullfight_result(random &random_gen) {
            draw_cards(random_gen);
        }

        asset get_payout(const bullfight::bet &bet_item) {
            return (5 + get_pay_rate(bet_item.bet_type)) * bet_item.bet / 5;
        }

        uint64_t get_pay_rate(uint8_t bet_type) {
            switch (bet_type) {
                case 1:
                    return player1_rate;
                case 2:
                    return player2_rate;
                case 3:
                    return player3_rate;
                case 4:
                    return player4_rate;
                default:
                    return 0;
            }
        }

        void draw_cards(random& random_gen) {
            card_t deck[NUM_CARDS];
            for (int i=0; i<NUM_CARDS; i++) {
                deck[i] = i;
            }
            draw_hand(random_gen, deck, banker_cards, 0, banker_type);
            banker_type = get_hand_type(banker_cards, banker_card_values);

            result = banker_type;
            handle_player(random_gen, deck, player1_cards, 5, player1_type, player1_rate);
            handle_player(random_gen, deck, player2_cards, 10, player2_type, player2_rate);
            handle_player(random_gen, deck, player3_cards, 15, player3_type, player3_rate);
            handle_player(random_gen, deck, player4_cards, 20, player4_type, player4_rate);
        }

        void handle_player(random& random_gen, card_t cards[], std::vector<card_t>& drawn,
            uint8_t already_drawn, uint8_t& hand_type, int64_t& pay_rate) {
            draw_hand(random_gen, cards, drawn, already_drawn, hand_type);

            std::vector<uint8_t> card_values;
            hand_type = get_hand_type(drawn, card_values);
            if (hand_type > banker_type) {
                pay_rate = get_hand_pay_rate(hand_type);
            } else if (banker_type != NO_BULL) {
                pay_rate = -get_hand_pay_rate(banker_type);
            } else {
                pay_rate = compare_hand_value(drawn, banker_cards);
            }
            roundResult <<= 1;
            roundResult |= pay_rate > 0 ? 1 : 0;
            result <<= 4;
            result |= hand_type;
        }

        static uint64_t compare_hand_value(std::vector<card_t>& player_hand, std::vector<card_t>& banker_hand) {
            uint8_t banker_value = card_value(banker_hand[4]);
            uint8_t player_value = card_value(player_hand[4]);
            if (player_value > banker_value) {
                return 1;
            } else if (player_value < banker_value) {
                return -1;
            } else {
                uint8_t banker_suit = card_suit(banker_hand[4]);
                uint8_t player_suit = card_suit(player_hand[4]);
                if (player_suit > banker_suit) {
                    return 1;
                } else {
                    return -1;
                }
            }
        }


        static uint8_t get_hand_pay_rate(uint8_t hand_type) {
            switch (hand_type) {
                case BULL_8:
                case BULL_9:
                    return 2;
                case BULL_10:
                    return 3;
                case FOUR_OF_A_KIND:
                case ALL_HEADS:
                    return 4;
                case SMALL_BULL:
                    return 5;
                default:
                    return 1;
            }
        }

        static void draw_hand(random& random_gen, card_t cards[], std::vector<card_t>& drawn, uint8_t already_drawn,
            uint8_t& hand_type) {
            uint64_t random_value = random_gen.generator(0);
            for (uint8_t i = 0; i < 5; i++) {
                uint8_t remaining_cards = 52 - already_drawn - i;
                uint8_t draw_index = random_value % remaining_cards;
                random_value /= remaining_cards;

                drawn.push_back(cards[draw_index]);
                cards[draw_index] = cards[remaining_cards - 1];
            }
            sort_by_value(drawn);
        }

        static uint8_t get_hand_type(std::vector<card_t>& cards, std::vector<uint8_t>& card_values) {
            for (int i=0; i<5; i++) {
                card_values.push_back(card_value(cards[i]));
            }

            uint8_t sum = 0;
            bool all_heads = true;
            uint8_t same_value_count = 1;
            uint8_t last_value = 14;

            for(uint8_t i = 0; i < 5; i++) {
                uint8_t card = card_values[i];
                sum += card;
                if(card <= 10) {
                    all_heads = false;
                }
                if(card == last_value) {
                    same_value_count++;
                } else {
                    last_value = card;
                    if (same_value_count < 4) {
                        same_value_count = 1;
                    }
                }
            }

            if (sum < 10) {
                return SMALL_BULL;
            }
            if (all_heads) {
                return ALL_HEADS;
            }
            if (same_value_count >= 4) {
                if (card_values[0] != card_values[1]) {
                    card_t value = cards[0];
                    cards.erase(cards.begin());
                    cards.push_back(value);
                }
                return FOUR_OF_A_KIND;
            }

            uint8_t value_array[5];
            for (int i=0; i<5; i++) {
                value_array[i] = min(card_values[i], (uint8_t) 10);
            }

            for(int i = 0; i < 3; i++) {
                for(int j = i + 1; j < 4; j++) {
                    for (int k = j+1; k < 5; k++) {
                        if ((value_array[i] + value_array[j] + value_array[k]) % 10 == 0) {
                            value_array[i] = 0;
                            value_array[j] = 0;
                            value_array[k] = 0;

                            swap_card(cards, i, 0);
                            swap_card(cards, j, 1);
                            swap_card(cards, k, 2);

                            uint8_t hand_type = 0;
                            for (uint8_t l = 0; l < 5; l++) {
                                hand_type += value_array[l];
                            }
                            hand_type = hand_type % 10;
                            if (hand_type == 0) {
                                return BULL_10;
                            }
                            return hand_type;
                        }
                    }
                }
            }
            return 0;
        }

        static void swap_card(std::vector<card_t>& cards, size_t source, size_t target) {
            card_t temp = cards[target];
            cards[target] = cards[source];
            cards[source] = temp;
        }

        void update_game(bullfight::game &game) {
            game.banker_cards = banker_cards;
            game.player1_cards = player1_cards;
            game.player2_cards = player2_cards;
            game.player3_cards = player3_cards;
            game.player4_cards = player4_cards;
        }

        void set_receipt(bullfight &contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, { contract.get_self(), name("active") }, { game_id, seed,
                cards_to_string(banker_cards), cards_to_string(player1_cards), player1_rate,
                cards_to_string(player2_cards), player2_rate,  cards_to_string(player3_cards),
                player3_rate, cards_to_string(player4_cards), player4_rate});
        }
    };

    void bullfight::receipt(uint64_t game_id, capi_checksum256 seed, string banker_cards, string player1_cards,
                            int64_t player1_rate, string player2_cards, int64_t player2_rate, string player3_cards,
                            int64_t player3_rate, string player4_cards, int64_t player4_rate) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(bullfight, Bull Fight, bullfight_result, 1)
};
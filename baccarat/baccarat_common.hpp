#pragma once

#include <eosiolib/eosio.hpp>
#include <vector>
#include "../common/random.hpp"

#define card_t uint16_t
#include "../common/cards.hpp"

#define G_ID_START                  101
#define G_ID_RESULT_ID              101
#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_HISTORY_ID             104
#define G_ID_END                    104

#define GAME_LENGTH                 45
#define GAME_RESOLVE_TIME           15

#define RESULT_SIZE                 100
#define HISTORY_SIZE                100

#define NUM_CARDS                   52 * 8


namespace godapp {
    uint8_t card_point(card_t card) {
        uint8_t value = card_value(card);
        return value > 9 ? 0 : value;
    }

    uint8_t cards_point(const std::vector<card_t>& cards) {
        uint8_t result = 0;
        for (card_t card: cards) {
            result += card_point(card);
        }
        return result % 10;
    }

    bool banker_draw_third_card(uint8_t banker_point, uint8_t player_third_card) {
        switch (banker_point) {
            case 0:
            case 1:
            case 2:
                return true;
            case 3:
                return player_third_card != 8;
            case 4:
                return player_third_card >= 2 && player_third_card <= 7;
            case 5:
                return player_third_card >= 4 && player_third_card <= 7;
            case 6:
                return player_third_card == 6 || player_third_card == 7;
            default:
                return false;
        }
    }

    void draw_cards(std::vector<card_t>& banker_cards, uint8_t& banker_point,
                    std::vector<card_t>& player_cards, uint8_t& player_point,
                    random& random_gen) {
        std::vector<card_t> cards;

        add_card(random_gen, banker_cards, cards, NUM_CARDS);
        add_card(random_gen, player_cards, cards, NUM_CARDS);

        add_card(random_gen, banker_cards, cards, NUM_CARDS);
        add_card(random_gen, player_cards, cards, NUM_CARDS);

        banker_point = cards_point(banker_cards);
        player_point = cards_point(player_cards);

        if (banker_point < 8 && player_point < 8) {
            if (player_point < 6) {
                uint8_t player_third_card = card_point(add_card(random_gen, player_cards, cards, NUM_CARDS));
                player_point = cards_point(player_cards);

                if (banker_draw_third_card(banker_point, player_third_card)) {
                    add_card(random_gen, banker_cards, cards, NUM_CARDS);
                    banker_point = cards_point(banker_cards);
                }
            } else if (banker_point < 6) {
                add_card(random_gen, banker_cards, cards, NUM_CARDS);
                banker_point = cards_point(banker_cards);
            }
        }
    }
}
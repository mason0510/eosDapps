#pragma once

#include <eosiolib/eosio.hpp>
#include "./constants.hpp"
#include "random.hpp"
#include <string>

#define CARD_SUIT_SPADE 0
#define CARD_SUIT_HEART 1
#define CARD_SUIT_DIAMOND 2
#define CARD_SUIT_CLUB 3

#define NUM_SUITS 4
#define CARDS_PER_SUIT 13

namespace godapp {
    using namespace std;

    uint8_t card_suit(uint8_t card) {
        return (card / CARDS_PER_SUIT) % NUM_SUITS;
    }

    uint8_t card_value(uint8_t card) {
        return card % CARDS_PER_SUIT + 1;
    }

    bool is_A(uint8_t card) {
        return card_value(card) == 1;
    }

    bool is_ten(uint8_t card) {
        return card_value(card) >= 10;
    }

    uint8_t draw_random_card(random& random_gen, const vector<uint8_t>& exclude, int max_number) {
        size_t total_dealt = exclude.size();
        auto card = (uint8_t) random_gen.generator(max_number - total_dealt);

        for(uint8_t i: exclude) {
            if (card >= i) {
                card++;
            } else {
                break;
            }
        }
        return card;
    }

    string card_suite_str(uint8_t card){
        switch (card_suit(card)) {
            case CARD_SUIT_SPADE:
                return "♠";
            case CARD_SUIT_HEART:
                return "♥";
            case CARD_SUIT_DIAMOND:
                return "♦";
            default:
                return "♣";
        }
    }

    string card_value_str(uint8_t card){
        switch (card % CARDS_PER_SUIT) {
            case 0:
                return "A";
            case 1:
                return "2";
            case 2:
                return "3";
            case 3:
                return "4";
            case 4:
                return "5";
            case 5:
                return "6";
            case 6:
                return "7";
            case 7:
                return "8";
            case 8:
                return "9";
            case 9:
                return "10";
            case 10:
                return "J";
            case 11:
                return "Q";
            default:
                return "K";
        }
    }

    string card_to_string(uint8_t card) {
        return card_suite_str(card) + card_value_str(card);
    }

    string cards_to_string(const vector<uint8_t>& cards) {
        string ret;
        size_t len = cards.size();
        for (size_t i=0; i<len; i++) {
            ret += card_to_string(cards[i]);
            if (i + 1 < len) {
                ret += ";";
            }
        }
        return ret;
    }
}
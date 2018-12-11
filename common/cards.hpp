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
#define ACE_HIGH_VALUE 14

namespace godapp {
    using namespace std;

    card_t card_suit(card_t card) {
        return (card / CARDS_PER_SUIT) % NUM_SUITS;
    }

    uint8_t card_value(card_t card) {
        return card % CARDS_PER_SUIT + 1;
    }

    uint8_t card_value_with_ace(card_t card) {
        uint8_t point = card_value(card);
        return point == 1 ? ACE_HIGH_VALUE : point;
    }

    bool is_A(card_t card) {
        return card_value(card) == 1;
    }

    bool is_ten(card_t card) {
        return card_value(card) >= 10;
    }

    card_t draw_random_card(random& random_gen, const vector<card_t>& exclude, card_t max_number) {
        size_t total_dealt = exclude.size();
        auto card = (card_t) random_gen.generator(max_number - total_dealt);

        for(card_t i: exclude) {
            if (card >= i) {
                card++;
            } else {
                break;
            }
        }
        return card;
    }

    card_t add_card(random& random_gen, vector<card_t>& target, vector<card_t>& existing, card_t max_number) {
        sort(existing.begin(), existing.end());

        card_t card = draw_random_card(random_gen, existing, max_number);
        target.push_back(card);
        existing.push_back(card);

        return card;
    }

    void add_cards(random& random_gen, vector<card_t>& target, vector<card_t>& existing, uint8_t count, card_t max_number) {
        for (uint8_t i=0; i<count; i++) {
            add_card(random_gen, target, existing, max_number);
        }
    }

    string card_suite_str(card_t card){
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

    string card_value_str(card_t card){
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

    string card_to_string(card_t card) {
        return card_suite_str(card) + card_value_str(card);
    }

    string cards_to_string(const vector<card_t>& cards) {
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
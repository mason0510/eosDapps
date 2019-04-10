#pragma once

#include <eosiolib/eosio.hpp>
#include "./constants.hpp"
#include "random.hpp"
#include <string>


#define CARD_SUIT_DIAMOND 0
#define CARD_SUIT_CLUB 1
#define CARD_SUIT_HEART 2
#define CARD_SUIT_SPADE 3

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

    /**
     * Get the value of a card, with Ace being the largest 14
     */
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

    /**
     * Get a random card from a deck, with excluded cards removed
     * @param random_gen The random number generator
     * @param exclude Cards excluded, expected to be sorted
     * @param max_number Max number of cards to pull
     * @return
     */
    card_t draw_random_card(random& random_gen, vector<card_t>& exclude, card_t max_number) {
        size_t total_dealt = exclude.size();
        // draw a card from the remaining deck
        auto card = (card_t) random_gen.generator(max_number - total_dealt);

        // sort the excluded cards based on card number
        sort(exclude.begin(), exclude.end());
        // then, from the beginning of the deck, bump the card number based on the number of cards already drawn
        for(card_t i: exclude) {
            if (card >= i) {
                card++;
            } else {
                break;
            }
        }
        return card;
    }

    /**
     * Add a card to the target list, excluding existing cards
     * @param random_gen Random number generator
     * @param target Target list to add the card
     * @param existing Cards already pulled
     * @param max_number Max number for card that can be drawn
     * @return the card drawn
     */
    card_t add_card(random& random_gen, vector<card_t>& target, vector<card_t>& existing, card_t max_number) {
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

    struct value_sort {
        bool operator()(const card_t& x, const card_t& y) const {return card_value(x) < card_value(y);}
    };

    void sort_by_value(vector<card_t>& cards) {
        sort(cards.begin(), cards.end(), value_sort());
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
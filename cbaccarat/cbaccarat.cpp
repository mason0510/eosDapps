#include "../cbaccarat/cbaccarat.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     3
#define BET_BANKER_PAIR             4
#define BET_PLAYER_PAIR             5

namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(cbaccarat)

    string result_to_string(uint8_t result) {
        switch (result & 7) {
            case BET_BANKER_WIN:
                return "Banker Wins";
            case BET_PLAYER_WIN:
                return "Player Wins";
            case BET_TIE:
                return "Tie";
            default:
                return "";
        }
    }

    class cbaccarat_result {
    public:
        vector<card_t> banker_cards, player_cards;
        bool banker_pair, player_pair;
        uint8_t banker_point, player_point;
        uint8_t result;

        cbaccarat_result() {
            draw_cards(banker_cards, banker_point, player_cards, player_point);

            if (player_point > banker_point) {
                result = BET_BANKER_WIN;
            } else if (banker_point > player_point) {
                result = BET_BANKER_WIN;
            } else {
                result = BET_TIE;
            }

            banker_pair = card_value(banker_cards[0]) == card_value(banker_cards[1]);
            player_pair = card_value(player_cards[0]) == card_value(player_cards[1]);
        }

        asset get_payout(const cbaccarat::bet& bet_item) {
            asset bet = bet_item.bet;
            switch (bet_item.bet_type) {
                case BET_BANKER_WIN:
                    if (result == BET_BANKER_WIN) {
                        return bet * 195 / 100;
                    } else if (result == BET_TIE) {
                        return bet;
                    } else {
                        return asset(0, bet.symbol);
                    }
                case BET_PLAYER_WIN:
                    if (result == BET_PLAYER_WIN) {
                        return bet * 2;
                    } else if (result == BET_TIE) {
                        return bet;
                    } else {
                        return asset(0, bet.symbol);
                    }
                case BET_TIE:
                    return result == BET_TIE ? (bet * 9) : asset(0, bet.symbol);
                case BET_BANKER_PAIR:
                    return banker_pair ? (bet * 12) : asset(0, bet.symbol);
                case BET_PLAYER_PAIR:
                    return player_pair ? (bet * 12) : asset(0, bet.symbol);
                default:
                    return asset(0, bet.symbol);
            }
        }

        void update_game(cbaccarat::game& game) {
            game.player_cards = player_cards;
            game.banker_cards = banker_cards;
        }

        void set_receipt(cbaccarat& contract, uint64_t game_id) {
            SEND_INLINE_ACTION(contract, receipt, {contract.get_self(), name("active")},
                               {game_id, cards_to_string(player_cards), player_point,
                                   cards_to_string(banker_cards), banker_point, result_to_string(result)});
        }
    };

    void cbaccarat::receipt(uint64_t game_id, string player_cards, uint8_t player_point, string banker_cards, uint8_t banker_point, string result) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(cbaccarat, "Baccarat Classic", cbaccarat_result)
};
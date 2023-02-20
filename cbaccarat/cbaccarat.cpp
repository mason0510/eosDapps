#include "../cbaccarat/cbaccarat.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     4
#define BET_BANKER_PAIR             8
#define BET_PLAYER_PAIR             16

namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(cbaccarat)

    class cbaccarat_result {
    public:
        vector<card_t> banker_cards, player_cards;
        bool banker_pair, player_pair;
        uint8_t banker_point, player_point;
        uint8_t game_result;
        uint8_t result;
        uint8_t roundResult;

        cbaccarat_result(random &random_gen) {
            draw_cards(banker_cards, banker_point, player_cards, player_point, random_gen);

            if (player_point > banker_point) {
                game_result = BET_PLAYER_WIN;
            } else if (banker_point > player_point) {
                game_result = BET_BANKER_WIN;
            } else {
                game_result = BET_TIE;
            }

            banker_pair = card_value(banker_cards[0]) == card_value(banker_cards[1]);
            player_pair = card_value(player_cards[0]) == card_value(player_cards[1]);

            result = game_result | (banker_pair ? BET_BANKER_PAIR : 0) | (player_pair ? BET_PLAYER_PAIR : 0);
            roundResult = result;
        }

        asset get_payout(const cbaccarat::bet &bet_item) {
            asset bet = bet_item.bet;
            switch (bet_item.bet_type) {
                case BET_BANKER_WIN:
                    if (game_result == BET_BANKER_WIN) {
                        return bet * 195 / 100;
                    } else if (game_result == BET_TIE) {
                        return bet;
                    } else {
                        return asset(0, bet.symbol);
                    }
                case BET_PLAYER_WIN:
                    if (game_result == BET_PLAYER_WIN) {
                        return bet * 2;
                    } else if (game_result == BET_TIE) {
                        return bet;
                    } else {
                        return asset(0, bet.symbol);
                    }
                case BET_TIE:
                    return game_result == BET_TIE ? (bet * 9) : asset(0, bet.symbol);
                case BET_BANKER_PAIR:
                    return banker_pair ? (bet * 12) : asset(0, bet.symbol);
                case BET_PLAYER_PAIR:
                    return player_pair ? (bet * 12) : asset(0, bet.symbol);
                default:
                    return asset(0, bet.symbol);
            }
        }

        string result_string() {
            switch (game_result) {
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

        void update_game(cbaccarat::game &game) {
            game.player_cards = player_cards;
            game.banker_cards = banker_cards;
        }

        void set_receipt(cbaccarat &contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, { contract.get_self(), name("active") }, { game_id, seed,
                                   cards_to_string(player_cards), player_point,
                                   cards_to_string(banker_cards), banker_point, result_string() });
        }
    };

    void cbaccarat::receipt(uint64_t game_id, capi_checksum256 seed, string player_cards, uint8_t player_point,
        string banker_cards,uint8_t banker_point, string result) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(cbaccarat, Baccarat Classic, cbaccarat_result, 1)
};
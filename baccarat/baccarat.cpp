#include "../baccarat/baccarat.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     3
#define BET_DRAGON                  4
#define BET_PANDA                   5

namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(baccarat)

    class baccarat_result {
    public:
        vector<card_t> banker_cards, player_cards;
        uint8_t banker_point, player_point;
        const uint8_t*  payout_array;
        uint8_t result;
        uint8_t roundResult;

        static constexpr uint8_t PAYOUT_MATRIX[5][5] = {
            {2,     0,      0,      0,      0}, // BANKER
            {0,     2,      0,      0,      0}, // PLAYER
            {1,     1,      9,      0,      0}, // TIE
            {1,     0,      0,      41,     0}, // DRAGON
            {0,     2,      0,      0,      26} // PANDA
        };
        //      BANKER, PLAYER, TIE,    DRAGON, PANDA

        baccarat_result(random& random_gen) {
            draw_cards(banker_cards, banker_point, player_cards, player_point, random_gen);

            result = 0;
            if (player_point > banker_point) {
                if (player_point == 8 && player_cards.size() == 3) {
                    result = BET_PANDA;
                } else {
                    result = BET_PLAYER_WIN;
                }
            } else if (banker_point > player_point) {
                if (banker_point == 7 && banker_cards.size() == 3) {
                    result = BET_DRAGON;
                } else {
                    result = BET_BANKER_WIN;
                }
            } else {
                result = BET_TIE;
            }
            roundResult = result;
            payout_array = PAYOUT_MATRIX[result - 1];
        }

        asset get_payout(const baccarat::bet& bet_item) {
            return bet_item.bet * payout_array[bet_item.bet_type - 1];
        }

        string result_string() {
            switch (result) {
                case BET_BANKER_WIN:
                    return "Banker Wins";
                case BET_PLAYER_WIN:
                    return "Player Wins";
                case BET_TIE:
                    return "Tie";
                case BET_DRAGON:
                    return "Dragon 7";
                case BET_PANDA:
                    return "Panda 8";
                default:
                    return "";
            }
        }

        void update_game(baccarat::game& game) {
            game.player_cards = player_cards;
            game.banker_cards = banker_cards;
        }

        void set_receipt(baccarat& contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, {contract.get_self(), name("active")},
                {game_id, seed, cards_to_string(player_cards), player_point,
                 cards_to_string(banker_cards), banker_point, result_string()});
        }
    };

    void baccarat::receipt(uint64_t game_id, capi_checksum256 seed, string player_cards, uint8_t player_point, string banker_cards, uint8_t banker_point, string result) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(baccarat, EZ Baccarat, baccarat_result, 1)
};
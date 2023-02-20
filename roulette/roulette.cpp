#include "../roulette/roulette.hpp"
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

#define GAME_LENGTH                 45
#define GAME_RESOLVE_TIME           15

#define RESULT_SIZE                 60
#define HISTORY_SIZE                40

#define BET_NUMBER_ZERO             1
#define BET_NUMBER_END              37
#define BET_EVEN                    38
#define BET_ODD                     39
#define BET_LARGE                   40
#define BET_SMALL                   41
#define BET_FRONT                   42
#define BET_MID                     43
#define BET_BACK                    44
#define BET_LINE_ONE                45
#define BET_LINE_TWO                46
#define BET_LINE_THREE              47
#define BET_RED                     48
#define BET_BLACK                   49


namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(roulette)

    class roulette_result {
    public:
        uint8_t result;
        uint8_t roundResult;
        bool is_red;

        roulette_result(random& random_gen) {
            result = random_gen.generator(BET_NUMBER_END);
            roundResult = result;
            is_red = IS_RED[result];
        }

        bool IS_RED[BET_NUMBER_END] = {
            false,              // 0
            true, false, true,  // 1
            false, true, false, // 4
            true, false, true,  // 7
            false, false, true, // 10
            false, true, false, // 13
            true, false, true,  // 16
            true, false, true,  // 19
            false, true, false, // 22
            true, false, true,  // 25
            false, false, true, // 28
            false, true, false, // 31
            true, false, true   // 34
        };

        asset get_payout(const roulette::bet& bet_item) {
            uint8_t bet_type = bet_item.bet_type;
            uint8_t pay_rate = 0;
            if (result == 0) {
                pay_rate = (bet_type == BET_NUMBER_ZERO) ? 36 : 0;
            } else {
                switch (bet_type) {
                    case BET_EVEN:
                        pay_rate = (result % 2 == 0) ? 2 : 0;
                        break;
                    case BET_ODD:
                        pay_rate = (result % 2 == 1) ? 2 : 0;
                        break;
                    case BET_LARGE:
                        pay_rate = (result > 18) ? 2 : 0;
                        break;
                    case BET_SMALL:
                        pay_rate = (result <= 18) ? 2 : 0;
                        break;
                    case BET_FRONT:
                        pay_rate = (result <= 12) ? 3 : 0;
                        break;
                    case BET_MID:
                        pay_rate = (result > 12 && result <= 24) ? 3 : 0;
                        break;
                    case BET_BACK:
                        pay_rate = (result > 24) ? 3 : 0;
                        break;
                    case BET_LINE_ONE:
                        pay_rate = (result % 3 == 0) ? 3 : 0;
                        break;
                    case BET_LINE_TWO:
                        pay_rate = (result % 3 == 2) ? 3 : 0;
                        break;
                    case BET_LINE_THREE:
                        pay_rate = (result % 3 == 1) ? 3 : 0;
                        break;
                    case BET_RED:
                        pay_rate = is_red ? 2 : 0;
                        break;
                    case BET_BLACK:
                        pay_rate = is_red ? 0 : 2;
                        break;
                    default:
                        pay_rate = (result == bet_type - 1) ? 36 : 0;
                }
            }

            return bet_item.bet * pay_rate;
        }

        void update_game(roulette::game& game) {
            game.result = result;
        }

        void set_receipt(roulette& contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, {contract.get_self(), name("active")}, {game_id, seed, result});
        }
    };

    void roulette::receipt(uint64_t game_id, capi_checksum256 seed, uint8_t result) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(roulette, Roulette, roulette_result, 1)
};
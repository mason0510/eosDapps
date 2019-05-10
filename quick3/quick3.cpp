#include "../quick3/quick3.hpp"

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

#define GAME_LENGTH                 60
#define GAME_RESOLVE_TIME           12

#define RESULT_SIZE                 100
#define HISTORY_SIZE                50


#define BET_NUMBER_THREE            3
#define BET_LARGE                   19
#define BET_SMALL                   20
#define BET_ODD                     21
#define BET_EVEN                    22
#define BET_PAIR                    23
#define BET_STRAIGHT                24
#define BET_THREE_OF_A_KIND         25



namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(quick3)

    class quick3_result {
    public:
        uint64_t result = 0;
        uint64_t roundResult = 0;
        uint8_t sum = 0;
        std::vector<uint8_t> dices;

        bool threeOfAKind = true;
        bool pair = false;
        bool straight = true;

        static constexpr uint64_t SUM_PAY_RATE_ARRAY[19] = {
            0, 0, 0, 19008, 6336, 3168, 1901, 1267, 904, 760,
            704, 704, 760, 904, 1267, 1901, 3168, 6336, 19008
        };

        quick3_result(random& random_gen) {
            uint8_t rng = random_gen.generator(216);
            std::vector<uint8_t> results;

            result = rng;
            roundResult = rng;

            for (int i=0; i<3; i++) {
                uint8_t dice = rng % 6 + 1;
                results.push_back(dice);

                sum += dice;
                rng /= 6;
            }
            dices = results;

            sort(results.begin(), results.end());
            uint8_t last = results[0];

            for (int i=1; i<3; i++) {
                uint8_t value = results[i];
                if (value != last) {
                    threeOfAKind = false;
                } else {
                    pair = true;
                }
                if (value != last + 1) {
                    straight = false;
                }
                last = value;
            }
        }

        asset get_payout(const quick3::bet& bet_item) {
            uint8_t bet_type = bet_item.bet_type;
            uint64_t pay_rate = 0;
            if (bet_type <= 18) {
                if (bet_type == sum) {
                    pay_rate = SUM_PAY_RATE_ARRAY[bet_type];
                }
            } else {
                switch (bet_type) {
                    case BET_LARGE:
                        pay_rate = sum > 10 ? 196 : 0;
                        break;
                    case BET_SMALL:
                        pay_rate = sum <= 10 ? 196 : 0;
                        break;
                    case BET_ODD:
                        pay_rate = sum % 2 == 1 ? 196 : 0;
                        break;
                    case BET_EVEN:
                        pay_rate = sum % 2 == 0 ? 196 : 0;
                        break;
                    case BET_PAIR:
                        pay_rate = pair ? 196 : 0;
                        break;
                    case BET_STRAIGHT:
                        pay_rate = straight ? 792 : 0;
                        break;
                    case BET_THREE_OF_A_KIND:
                        pay_rate = threeOfAKind ? 3168 : 0;
                        break;
                    default:
                        pay_rate = 0;
                }
            }

            return bet_item.bet * pay_rate / 100;
        }

        void update_game(quick3::game& game) {
            game.result = dices;
        }

        void set_receipt(quick3& contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, {contract.get_self(), name("active")}, {game_id, seed, dices})
        }
    };

    void quick3::receipt(uint64_t game_id, capi_checksum256 seed, std::vector<uint8_t> result) {
        require_auth(_self);
        require_recipient(_self);
    }

    DEFINE_REVEAL_FUNCTION(quick3, quick3, quick3_result, 1)
};
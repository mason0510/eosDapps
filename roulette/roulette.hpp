#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.h>
#include <eosiolib/transaction.hpp>


#include "../common/constants.hpp"
#include "../common/contracts.hpp"
#include "../common/random.hpp"
#include "../common/round_based_contract.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT roulette: public contract {
    public:
        DEFINE_GLOBAL_TABLE
        DEFINE_GAMES_TABLE(uint8_t result;)
        DEFINE_BETS_TABLE
        DEFINE_RESULTS_TABLE
        DEFINE_HISTORY_TABLE
        DEFINE_BET_AMOUNT_TABLE
        DEFINE_RANDOM_KEY_TABLE

        ACTION receipt(uint64_t game_id, capi_checksum256 seed, uint8_t result);
        DECLARE_STANDARD_ACTIONS(roulette)
    };

    EOSIO_ABI_EX(roulette, STANDARD_ACTIONS(receipt))
}
